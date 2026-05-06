/**
 * @file drivers/timer.h
 * @brief Timer management module for system timing and delays (moved to drivers/)
 *
 * NOTE: This file was moved from the top-level RL_SPORT directory into
 * the drivers/ subdirectory to group hardware drivers. API and behavior
 * remain unchanged.
 */
#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

/**
 * @brief Initialize Timer0 for 1ms periodic interrupt.
 */
void Timer_Init(void);

/**
 * @brief Blocking delay in milliseconds
 */
void delay_ms(uint32_t ms);

/**
 * @brief Millisecond tick counter accessor
 */
uint32_t get_ticks_ms(void);

/**
 * @brief Elapsed ms since ref_time
 */
uint32_t get_elapsed_ms(uint32_t ref_time);

/**
 * @brief Timeout helper
 */
uint8_t is_timeout(uint32_t start_time, uint32_t timeout_ms);

#endif // _TIMER_H_
