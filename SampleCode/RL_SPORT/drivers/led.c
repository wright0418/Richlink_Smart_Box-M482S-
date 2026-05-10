/**
 * @file drivers/led.c
 * @brief Green LED driver for PWM-style blink control.
 *
 * Implements a simple software PWM blink engine driven by the 1ms system
 * tick. The LED may be forced on/off directly or configured with a
 * frequency/duty cycle for heartbeat and status indication patterns.
 */
#include "led.h"
#if defined(__has_include)
#if __has_include("NuMicro.h")
#include "NuMicro.h"
#else
/* minimal stubbed environment for host/static analysis */
typedef struct
{
    volatile uint32_t DOUT;
} GPIO_PORT_Type;
static GPIO_PORT_Type __PB_port = {0};
#define PB (&__PB_port)
#define BIT3 (1UL << 3)

#define GPIO_MODE_OUTPUT 0
#define GPIO_MODE_INPUT 0
#define GPIO_SetMode(port, bit, mode) ((void)0)

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
static inline int SysTick_Config(uint32_t ticks)
{
    (void)ticks;
    return 0;
}
static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}
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

/* forward declarations */
static void SetGreenLedRaw(uint8_t state);
static void Led_Update(void);

#define LED_MODE_TOLERANCE 0.001f
volatile uint32_t g_u32PeriodCounter = 0;
volatile uint32_t g_u32OnTimeCounter = 0;
volatile float g_LedFreqHz = 1;
volatile float g_LedDuty = 0.5f;
static volatile int8_t g_forceLedState = -1;

static volatile uint32_t g_u32PeriodTicksTarget = 0;
static volatile uint32_t g_u32OnTicks = 0;

/**
 * @brief Directly set the raw green LED GPIO state.
 * @param state 1 to turn the LED on, 0 to turn it off.
 *
 * This helper bypasses the software blink engine and writes the pin state
 * directly, leaving the blink engine counters unchanged.
 */
static void SetGreenLedRaw(uint8_t state)
{
    if (state)
        PB->DOUT |= BIT3;
    else
        PB->DOUT &= ~BIT3;
}

void SetGreenLed(uint8_t state)
{
    DBG_PRINT("[LED] SetGreenLed state=%u (force)\n", (unsigned)state);
    g_forceLedState = (int8_t)state;
    SetGreenLedRaw(state);
}

/**
 * @brief Update the software PWM blink engine at each timer tick.
 *
 * This function is driven by the timer ISR and handles both forced on/off
 * states and periodic PWM refresh based on the configured frequency and
 * duty cycle.
 */
static void Led_Update(void)
{
    if (g_forceLedState >= 0)
    {
        SetGreenLedRaw((uint8_t)g_forceLedState);
        return;
    }

    if (g_u32OnTimeCounter > 0)
    {
        SetGreenLedRaw(1);
        g_u32OnTimeCounter--;
    }
    else
    {
        SetGreenLedRaw(0);
    }

    if (g_u32PeriodTicksTarget == 0)
    {
        g_u32PeriodCounter = 0;
        g_u32OnTimeCounter = 0;
        return;
    }

    if (g_u32PeriodCounter >= g_u32PeriodTicksTarget)
    {
        g_u32PeriodCounter = 0;
        g_u32OnTimeCounter = g_u32OnTicks;
    }
}

void SetGreenLedMode(float freq, float duty)
{
    if (freq <= 0.0f)
    {
        if (g_u32PeriodTicksTarget != 0 || g_u32OnTicks != 0)
        {
            DBG_PRINT("[LED] SetGreenLedMode: freq<=0, turning LED off\n");
        }
        g_LedFreqHz = 0;
        g_LedDuty = 0;
        __disable_irq();
        g_u32PeriodTicksTarget = 0;
        g_u32OnTicks = 0;
        g_u32PeriodCounter = 0;
        g_u32OnTimeCounter = 0;
        __enable_irq();
        SetGreenLedRaw(0);
        return;
    }

    float d = duty;
    g_forceLedState = -1;
    if (d > 1.0f)
    {
        if (g_u32OnTimeCounter > 0)
            SetGreenLedRaw(1);
        else
            SetGreenLedRaw(0);
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

    DBG_PRINT("[LED] SetGreenLedMode apply freq=%.2f duty=%.2f\n", freq, d);

    float period_ms_f = 1000.0f / freq;
    uint32_t period_ms = (uint32_t)(period_ms_f + 0.5f);
    if (period_ms == 0)
        period_ms = 1;
    uint32_t on_ms = (uint32_t)(period_ms * d + 0.5f);

    g_LedFreqHz = freq;
    g_LedDuty = d;

    __disable_irq();
    g_u32PeriodTicksTarget = (uint32_t)((period_ms + (LED_TIMER_TICK_MS / 2)) / LED_TIMER_TICK_MS);
    if (g_u32PeriodTicksTarget == 0)
        g_u32PeriodTicksTarget = 1;
    g_u32OnTicks = (uint32_t)((on_ms + (LED_TIMER_TICK_MS / 2)) / LED_TIMER_TICK_MS);
    g_u32PeriodCounter = 0;
    g_u32OnTimeCounter = g_u32OnTicks;
    __enable_irq();
}

void SetGreenLedModePercent(float freq, uint8_t duty_percent)
{
    SetGreenLedMode(freq, (float)duty_percent);
}

void LedSetModePercent(float freq, uint8_t duty_percent)
{
    SetGreenLedModePercent(freq, duty_percent);
}

void Led_Init(void)
{
    GPIO_SetMode(PB, BIT3, GPIO_MODE_OUTPUT);
    SetGreenLedRaw(0);
}

void Led_SpdModeGpio(void)
{
    GPIO_SetMode(PB, BIT3, GPIO_MODE_INPUT);
}

void Led_TimerCallback(void)
{
    g_u32PeriodCounter++;
    Led_Update();
}
