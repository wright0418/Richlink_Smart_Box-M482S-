/**
 * @file pwm_timer.h
 * @brief Lightweight PWM and timer helper API
 *
 * This header exposes a small set of helpers to start/stop PWM outputs
 * and to use the common millisecond tick source shared with the LED
 * module. Implementations are platform-specific; the API focuses on
 * portability and clear parameter ranges.
 */

#ifndef _PWM_TIMER_H_
#define _PWM_TIMER_H_

#include <stdint.h>

/**
 * @brief Initialize timers and PWM resources used by modules.
 *
 * Safe to call multiple times; must be called before starting PWM on any
 * channel.
 */
void PWM_Timer_Init(void);

/**
 * @brief Start or reconfigure PWM on a channel.
 * @param channel Logical PWM channel (mapped to hardware in implementation).
 * @param freq_hz Frequency in Hz. Use 0 to stop PWM on the channel.
 * @param duty_percent Duty cycle 0..100 (0 = off, 100 = full on).
 */
void PWM_Start(uint8_t channel, uint32_t freq_hz, uint8_t duty_percent);

/**
 * @brief Stop PWM on the specified channel.
 * @param channel PWM channel to stop.
 */
void PWM_Stop(uint8_t channel);

/**
 * @brief Busy-wait delay using the shared LED tick source.
 * @param ms Milliseconds to delay (approximate if tick base != 1ms).
 *
 * Note: Uses busy-wait; not suitable for low-power sleep. Prefer event
 * driven waits in power-sensitive code.
 */
void pwm_delay_ms(uint32_t ms);

#endif // _PWM_TIMER_H_
