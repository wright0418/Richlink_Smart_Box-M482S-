/**
 * @file timer.h
 * @brief Timer management module for system timing and delays
 *
 * This module provides centralized timing services including:
 *  - Hardware timer initialization (Timer0)
 *  - Millisecond tick counter
 *  - Delay functions
 *  - Time measurement utilities
 */
#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

/**
 * @brief Initialize Timer0 for 1ms periodic interrupt.
 *
 * Configures Timer0 to generate interrupts at 1kHz (1ms period).
 * Must be called during system initialization before using delay
 * or tick functions.
 */
void Timer_Init(void);

/**
 * @brief Delay execution for specified milliseconds.
 * @param ms Number of milliseconds to delay.
 *
 * Blocking delay using the system tick counter. Accuracy depends
 * on Timer0 interrupt frequency (1ms).
 */
void delay_ms(uint32_t ms);

/**
 * @brief Get current system tick count in milliseconds.
 * @return Number of milliseconds since Timer_Init() was called.
 *
 * Wraps around after ~49.7 days (UINT32_MAX ms). Use delta
 * calculations for timeout/duration measurements to handle wrap.
 */
uint32_t get_ticks_ms(void);

/**
 * @brief Get elapsed time since a reference timestamp.
 * @param ref_time Reference timestamp (from get_ticks_ms()).
 * @return Milliseconds elapsed since ref_time.
 *
 * Handles uint32_t wraparound correctly using unsigned arithmetic.
 */
uint32_t get_elapsed_ms(uint32_t ref_time);

/**
 * @brief Check if timeout period has elapsed.
 * @param start_time Reference timestamp (from get_ticks_ms()).
 * @param timeout_ms Timeout period in milliseconds.
 * @return 1 if timeout elapsed, 0 otherwise.
 */
uint8_t is_timeout(uint32_t start_time, uint32_t timeout_ms);

#endif // _TIMER_H_
