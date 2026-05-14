/**
 * @file drivers/buzzer.c
 * @brief Buzzer timing and PWM control implementation.
 *
 * Uses Timer1 ISR to toggle the buzzer drive output and supports both
 * fixed-duration beeps and continuous tone output.
 */
#include "buzzer.h"
#include "NuMicro.h"
#include "../project_config.h"

void Buzzer_Init(void)
{
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);
    PC->DOUT &= ~BIT7;
    DBG_PRINT("[Buzzer] init PC7 output low\n");
}

void MCU_DLPS_GPIO(void)
{
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OPEN_DRAIN);
}

static volatile uint32_t buzzer_cycles = 0;
static volatile uint32_t buzzer_half_period_us = 0;

void BuzzerPlay(uint32_t freq, uint32_t time_ms)
{
    if (freq == 0 || time_ms == 0)
    {
        DBG_PRINT("[Buzzer] play ignored freq=%lu time_ms=%lu\n",
                  (unsigned long)freq,
                  (unsigned long)time_ms);
        return;
    }
    buzzer_half_period_us = (1000000U / freq) / 2U;
    buzzer_cycles = (uint32_t)(((uint64_t)freq * (uint64_t)time_ms * 2ULL) / 1000ULL);
    DBG_PRINT("[Buzzer] play freq=%luHz time=%lums cycles=%lu\n",
              (unsigned long)freq,
              (unsigned long)time_ms,
              (unsigned long)buzzer_cycles);
    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HXT, 0);
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, freq * 2U);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);
    TIMER_Start(TIMER1);
}

void Buzzer_Start(uint32_t freq)
{
    if (freq == 0)
    {
        DBG_PRINT("[Buzzer] start ignored freq=0\n");
        return;
    }
    buzzer_half_period_us = (1000000U / freq) / 2U;
    buzzer_cycles = 0xFFFFFFFFU;
    DBG_PRINT("[Buzzer] start continuous freq=%luHz\n", (unsigned long)freq);
    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HXT, 0);
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, freq * 2U);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);
    TIMER_Start(TIMER1);
}

void Buzzer_Stop(void)
{
    DBG_PRINT("[Buzzer] stop\n");
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
            PC->DOUT &= ~BIT7;
            Buzzer_Stop();
        }
    }
}
