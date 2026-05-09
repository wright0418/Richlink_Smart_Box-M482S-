/**
 * @file drivers/timer.c
 * @brief Timer management module implementation (moved to drivers/)
 */
#include "timer.h"
#include "NuMicro.h"
#include "../project_config.h"

/* System tick counter (incremented by Timer0 ISR at 1kHz) */
static volatile uint32_t g_system_ticks_ms = 0;

/* Forward declaration for LED update callback */
extern void Led_TimerCallback(void);

/*
 * Some library delay paths may transiently use SysTick.
 * Provide a project-level SysTick handler so we don't fall back to
 * startup weak default stubs during bring-up/debug.
 */
void SysTick_Handler(void)
{
    /* Intentionally empty */
}

void TMR0_IRQHandler(void)
{
    if (TIMER_GetIntFlag(TIMER0) == 1)
    {
        TIMER_ClearIntFlag(TIMER0);
        g_system_ticks_ms++;
        Led_TimerCallback();
    }
}

void Timer_Init(void)
{
    g_system_ticks_ms = 0;
    TIMER_Open(TIMER0, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER0);
    NVIC_EnableIRQ(TMR0_IRQn);
    TIMER_Start(TIMER0);
}

void delay_ms(uint32_t ms)
{
    uint32_t start = g_system_ticks_ms;
    while ((g_system_ticks_ms - start) < ms)
    {
        __WFI();
    }
}

uint32_t get_ticks_ms(void)
{
    return g_system_ticks_ms;
}

uint32_t get_elapsed_ms(uint32_t ref_time)
{
    return (g_system_ticks_ms - ref_time);
}

uint8_t is_timeout(uint32_t start_time, uint32_t timeout_ms)
{
    return (get_elapsed_ms(start_time) >= timeout_ms) ? 1 : 0;
}
