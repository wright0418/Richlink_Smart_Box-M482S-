/* pwm_timer.c - minimal PWM/timer helper (placeholder)
   This module intentionally keeps hardware ownership in led.c / buzzer.c
   to avoid regressing existing timer ownership. It provides a common API
   for future PWM migration. */

#include "pwm_timer.h"
#include "led.h" /* for delay/get_ticks */

void PWM_Timer_Init(void)
{
    /* noop for now - timers are initialized by modules that need them */
}

void PWM_Start(uint8_t channel, uint32_t freq_hz, uint8_t duty_percent)
{
    /* Implement when moving PWM ownership from modules into here. */
    (void)channel; (void)freq_hz; (void)duty_percent;
}

void PWM_Stop(uint8_t channel)
{
    (void)channel;
}

void pwm_delay_ms(uint32_t ms)
{
    delay_ms(ms);
}
