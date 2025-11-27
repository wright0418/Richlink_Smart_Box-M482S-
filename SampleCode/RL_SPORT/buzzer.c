/* buzzer.c - Buzzer control implementation */
#include "buzzer.h"
#include "NuMicro.h"

void Buzzer_Init(void)
{
    /* Configure PC7 as output for buzzer */
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);
    PC->DOUT &= ~BIT7;
}

void MCU_DLPS_GPIO(void)
{
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OPEN_DRAIN);
}

/* Simple non-blocking buzzer using TIMER1 to toggle the pin. */
static volatile uint32_t buzzer_cycles = 0;
static volatile uint32_t buzzer_half_period_us = 0;

void BuzzerPlay(uint32_t freq, uint32_t time_ms)
{
    if (freq == 0 || time_ms == 0)
        return;
    buzzer_half_period_us = (1000000U / freq) / 2U;
    buzzer_cycles = (uint32_t)freq * time_ms / 1000U * 2U; // half cycles
    /* Configure TIMER1 for periodic interrupt using half-period as tick */
    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HXT, 0);
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000); // frequency value replaced by period in us later
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);
    /* Start TIMER1; we'll use SysTick delay for approx half-period in handler if needed */
    TIMER_Start(TIMER1);
}

void Buzzer_Start(uint32_t freq)
{
    if (freq == 0)
        return;
    buzzer_half_period_us = (1000000U / freq) / 2U;
    buzzer_cycles = 0xFFFFFFFFU; // run until stopped
    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HXT, 0);
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);
    TIMER_Start(TIMER1);
}

void Buzzer_Stop(void)
{
    TIMER_Stop(TIMER1);
    TIMER_DisableInt(TIMER1);
    NVIC_DisableIRQ(TMR1_IRQn);
    PC->DOUT &= ~BIT7;
}

void TMR1_IRQHandler(void)
{
    if (TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
        if (buzzer_cycles > 0)
        {
            PC->DOUT ^= BIT7;
            buzzer_cycles--;
        }
        else
        {
            /* Stop when done */
            PC->DOUT &= ~BIT7;
            Buzzer_Stop();
        }
    }
}
