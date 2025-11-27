/* led.c - Green LED control implementation */
#include "led.h"
#if defined(__has_include)
#if __has_include("NuMicro.h")
#include "NuMicro.h"
#else
/* If the MCU device headers are not present in this build environment,
   provide lightweight stubs so host builds / static analysis won't fail.
   These stubs are in the #else branch so the real NuMicro.h is used on
   the target toolchain when available. */
/* minimal GPIO register stub */
typedef struct
{
    volatile uint32_t DOUT;
} GPIO_PORT_Type;
static GPIO_PORT_Type __PB_port = {0};
#define PB (&__PB_port)
#define BIT3 (1UL << 3)

/* minimal GPIO API stubs */
#define GPIO_MODE_OUTPUT 0
#define GPIO_MODE_INPUT 0
#define GPIO_SetMode(port, bit, mode) ((void)0)

/* minimal clock/timer/NVIC stubs */
#define TMR0_MODULE 0
#define CLK_CLKSEL1_TMR0SEL_HXT 0
#define TIMER0 0
#define TIMER_PERIODIC_MODE 0
#define TMR0_IRQn 0

static inline void CLK_EnableModuleClock(int x) { (void)x; }
static inline void CLK_SetModuleClock(int a, int b, int c)
{
    (void)a;
    (void)b;
    (void)c;
}
static inline void TIMER_Open(int a, int b, int c)
{
    (void)a;
    (void)b;
    (void)c;
}
static inline void TIMER_EnableInt(int a) { (void)a; }
static inline void TIMER_Start(int a) { (void)a; }
static inline int TIMER_GetIntFlag(int a)
{
    (void)a;
    return 0;
}
static inline void TIMER_ClearIntFlag(int a) { (void)a; }
static inline void NVIC_EnableIRQ(int a) { (void)a; }
static inline void NVIC_DisableIRQ(int a) { (void)a; }
/* Provide lightweight stubs for SysTick and global IRQ control in host builds */
static inline int SysTick_Config(uint32_t ticks)
{
    (void)ticks;
    return 0;
}
static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}
/* Provide a default SystemCoreClock in stub builds (overridden by real device headers) */
static uint32_t __stub_SystemCoreClock = 12000000;
#define SystemCoreClock (__stub_SystemCoreClock)
#endif
#else
#include "NuMicro.h"
#endif

#if defined(__has_include)
#if __has_include(<math.h>)
#include <math.h>
#else
static inline float fabsf(float value) { return value < 0.0f ? -value : value; }
#endif
#else
#include <math.h>
#endif

/* forward declarations to avoid implicit-declaration errors */
static void SetGreenLedRaw(uint8_t state);
static void Led_Update(void);

#define LED_MODE_TOLERANCE 0.001f
/* LED state variables (no longer need g_u32Ticks as timer.c provides it) */
volatile uint32_t g_u32PeriodCounter = 0;
volatile uint32_t g_u32OnTimeCounter = 0;
volatile float g_LedFreqHz = 1;
volatile float g_LedDuty = 0.5f;

/* Internal precomputed tick targets (milliseconds) to avoid float ops in ISR */
/* Internal precomputed tick targets (in units of LED_TIMER_TICK_MS). */
static volatile uint32_t g_u32PeriodTicksTarget = 0; /* period in ticks */
static volatile uint32_t g_u32OnTicks = 0;           /* on time in ticks */

/* LED module no longer manages timer init (handled by timer.c) */

static void SetGreenLedRaw(uint8_t state)
{
    if (state)
        PB->DOUT |= BIT3;
    else
        PB->DOUT &= ~BIT3;
}

void SetGreenLed(uint8_t state)
{
    SetGreenLedRaw(state);
}

// Update LED state: toggle according to period and duty
static void Led_Update(void)
{
    /* Use precomputed tick targets (set by SetGreenLedMode) to avoid
       floating-point work inside ISR context. g_u32OnTicks and
       g_u32PeriodTicksTarget are expressed in units of LED_TIMER_TICK_MS. */

    /* If on-time counter is non-zero, LED should be ON for that many ticks. */
    if (g_u32OnTimeCounter > 0)
    {
        SetGreenLedRaw(1);
        g_u32OnTimeCounter--;
    }
    else
    {
        SetGreenLedRaw(0);
    }

    /* If no blinking target configured, nothing else to update. */
    if (g_u32PeriodTicksTarget == 0)
    {
        /* ensure counters are zeroed */
        g_u32PeriodCounter = 0;
        g_u32OnTimeCounter = 0;
        return;
    }

    /* Advance period counter and when reaching the target, reload on-time */
    if (g_u32PeriodCounter >= g_u32PeriodTicksTarget)
    {
        g_u32PeriodCounter = 0;
        /* reload on-time from precomputed ticks */
        g_u32OnTimeCounter = g_u32OnTicks;
    }
}

void SetGreenLedMode(float freq, float duty)
{
    /* Normalize and validate inputs. duty may be passed as 0..1 or 0..100 (%).
       If duty > 1 treat as percent and convert to fraction. Clamp to [0,1].
       Precompute integer ticks (ms) to avoid float in ISR. */
    if (freq <= 0.0f)
    {
        /* turn LED off */
        g_LedFreqHz = 0;
        g_LedDuty = 0;
        /* disable blinking target and force LED off */
        __disable_irq();
        g_u32PeriodTicksTarget = 0;
        g_u32OnTicks = 0;
        g_u32PeriodCounter = 0;
        g_u32OnTimeCounter = 0;
        __enable_irq();
        SetGreenLedRaw(0);
        return;
    }

    /* normalize duty */
    float d = duty;
    if (d > 1.0f)
    {
        /* Reflect new state on the LED pin immediately so callers see the
           requested state even before the first timer interrupt occurs. */
        if (g_u32OnTimeCounter > 0)
            SetGreenLedRaw(1);
        else
            SetGreenLedRaw(0);
        /* treat as percentage */
        if (d > 100.0f)
            d = 100.0f;
        d = d / 100.0f;
    }
    if (d < 0.0f)
        d = 0.0f;
    if (d > 1.0f)
        d = 1.0f;

    if (g_u32PeriodTicksTarget != 0 &&
        fabsf(freq - g_LedFreqHz) < LED_MODE_TOLERANCE &&
        fabsf(d - g_LedDuty) < LED_MODE_TOLERANCE)
    {
        return;
    }

    /* compute integer period and on ticks (ms). Use rounding. */
    float period_ms_f = 1000.0f / freq;
    uint32_t period_ms = (uint32_t)(period_ms_f + 0.5f);
    if (period_ms == 0)
        period_ms = 1;
    uint32_t on_ms = (uint32_t)(period_ms * d + 0.5f);

    /* update visible values and precomputed targets atomically */
    g_LedFreqHz = freq;
    g_LedDuty = d;

    /* Use global IRQ disable to protect the shared state (SysTick-driven) */
    __disable_irq();
    /* store targets in tick units */
    g_u32PeriodTicksTarget = (uint32_t)((period_ms + (LED_TIMER_TICK_MS / 2)) / LED_TIMER_TICK_MS);
    if (g_u32PeriodTicksTarget == 0)
        g_u32PeriodTicksTarget = 1;
    g_u32OnTicks = (uint32_t)((on_ms + (LED_TIMER_TICK_MS / 2)) / LED_TIMER_TICK_MS);
    /* reset counters so new period starts immediately */
    g_u32PeriodCounter = 0;
    g_u32OnTimeCounter = g_u32OnTicks;
    __enable_irq();
}

void SetGreenLedModePercent(float freq, uint8_t duty_percent)
{
    SetGreenLedMode(freq, (float)duty_percent);
}

void Led_Init(void)
{
    /* Configure PB3 as output for green LED */
    GPIO_SetMode(PB, BIT3, GPIO_MODE_OUTPUT);
    SetGreenLedRaw(0);
    /* Timer initialization is now handled by timer.c module */
}

/* Note: delay_ms() and get_ticks_ms() are now provided by timer.h module */

/* Configure GPIO state used during SPD power-down for LED pin(s) */
void Led_SpdModeGpio(void)
{
    /* PB3 should be input in SPD mode to reduce leakage */
    GPIO_SetMode(PB, BIT3, GPIO_MODE_INPUT); // GREEN LED PB3
}

/**
 * @brief LED timer callback, called from Timer0 ISR (timer.c)
 *
 * This callback is invoked every 1ms by the timer module to update
 * LED blinking state.
 */
void Led_TimerCallback(void)
{
    g_u32PeriodCounter++;
    Led_Update();
}
