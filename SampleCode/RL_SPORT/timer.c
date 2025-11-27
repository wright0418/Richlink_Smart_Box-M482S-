/**
 * @file timer.c
 * @brief Timer management module implementation
 */
#include "timer.h"
#include "NuMicro.h"
#include "project_config.h"

/* System tick counter (incremented by Timer0 ISR at 1kHz) */
static volatile uint32_t g_system_ticks_ms = 0;

/* Forward declaration for LED update callback */
extern void Led_TimerCallback(void);

/**
 * @brief Timer0 interrupt handler.
 *
 * Called every 1ms by hardware. Increments the system tick counter
 * and calls LED module callback for blinking control.
 */
void TMR0_IRQHandler(void)
{
    if (TIMER_GetIntFlag(TIMER0) == 1)
    {
        /* Clear Timer0 interrupt flag */
        TIMER_ClearIntFlag(TIMER0);

        /* Increment system tick */
        g_system_ticks_ms++;

        /* Call LED module callback for blinking */
        Led_TimerCallback();
    }
}

void Timer_Init(void)
{
    /* Reset tick counter */
    g_system_ticks_ms = 0;

    /* Configure Timer0: Periodic mode, 1kHz (1ms period) */
    TIMER_Open(TIMER0, TIMER_PERIODIC_MODE, 1000);

    /* Enable Timer0 interrupt */
    TIMER_EnableInt(TIMER0);
    NVIC_EnableIRQ(TMR0_IRQn);

    /* Start Timer0 */
    TIMER_Start(TIMER0);
}

void delay_ms(uint32_t ms)
{
    uint32_t start = g_system_ticks_ms;
    /* Wait until the required time has elapsed */
    while ((g_system_ticks_ms - start) < ms)
    {
        /* Busy-wait (could add __WFI() for power saving) */
    }
}

uint32_t get_ticks_ms(void)
{
    return g_system_ticks_ms;
}

uint32_t get_elapsed_ms(uint32_t ref_time)
{
    /* Handles wraparound correctly using unsigned arithmetic */
    return (g_system_ticks_ms - ref_time);
}

uint8_t is_timeout(uint32_t start_time, uint32_t timeout_ms)
{
    return (get_elapsed_ms(start_time) >= timeout_ms) ? 1 : 0;
}
