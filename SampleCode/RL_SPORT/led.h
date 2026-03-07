/**
 * @file led.h
 * @brief Green LED control module (public API)
 *
 * This header exposes a small API for blinking and controlling the
 * GREEN LED on the board. It provides frequency/duty configuration
 * and lightweight timing helpers used by other modules.
 *
 * Usage contract:
 *  - Call Led_Init() early in startup to configure GPIO and timer.
 *  - Use SetGreenLedMode() to configure blinking; pass duty as a
 *    fraction (0.0..1.0) or use SetGreenLedModePercent() for 0..100.
 *  - The module uses a timer tick base defined by LED_TIMER_TICK_MS
 *    (default 1 ms) — ensure it matches the hardware timer setup if
 *    changed.
 */
#ifndef _LED_H_
#define _LED_H_
#include "project_config.h"
#include <stdint.h>

/* LED state shared variables (extern for observers/tests) */
extern volatile uint32_t g_u32OnTimeCounter;
extern volatile uint32_t g_u32PeriodCounter;
extern volatile float g_LedFreqHz;
extern volatile float g_LedDuty;

/* LED pin definition (module-owned) */
#define PIN_PIN_GREEN_LED PB3

/* LED default parameters */
#define LED_FREQ_DEFAULT 2
#define LED_DUTY_DEFAULT 50

/*
 * Module overview
 * ----------------
 * This module controls the GREEN LED (PB3) and provides a simple
 * periodic blinking API. Key points:
 *  - The LED blink is configured by frequency (Hz) and duty (fraction).
 *  - Consumers may pass duty either as a fraction (0.0 .. 1.0) to
 *    SetGreenLedMode, or use the explicit percent API
 *    SetGreenLedModePercent(freq, percent) where percent is 0..100.
 *  - A frequency <= 0 disables blinking and turns the LED off.
 *  - Timing calculations use the LED_TIMER_TICK_MS constant; see above.
 *
 * Thread-safety / ISR notes
 *  - SetGreenLedMode performs a short critical section (disables the
 *    timer IRQ briefly) while updating shared volatile state. This is
 *    intentionally short and safe for typical embedded usage.
 *  - The LED update function is driven from the timer ISR and must be
 *    fast; avoid calling heavy-weight code from the ISR.
 */

/* Timer tick base used by the LED module (milliseconds per tick).
    Default 1 means timer interrupt at 1ms (1000Hz). Change this if you
    reconfigure the hardware timer to a different tick period. */
#ifndef LED_TIMER_TICK_MS
#define LED_TIMER_TICK_MS 1
#endif

/* Derived timer frequency used to program the hardware timer.
    Provide both integer and float variants to avoid integer-division
    precision loss when used in floating-point calculations. */
#define LED_TIMER_FREQ_HZ_INT (1000 / LED_TIMER_TICK_MS)
#define LED_TIMER_FREQ_HZ_F (1000.0f / (float)LED_TIMER_TICK_MS)

// SetGreenLedRaw / Led_Update are static helpers used only within led.c;
// they do not require (and should not have) declarations in this header.
/* led.h - Green LED control module */

/* Avoid including device headers (NuMicro.h) here to keep header portable
    and to prevent static-analysis include-path issues. Device-specific types
    should be included in the corresponding .c file. */

/**
 * @brief Initialize LED module and required timer/GPIO resources.
 *
 * Safe to call multiple times; idempotent. Must be called before
 * configuring blinking with SetGreenLedMode/SetGreenLedModePercent.
 */
void Led_Init(void);

/**
 * @brief Set LED output state immediately.
 * @param state Non-zero = ON, zero = OFF.
 *
 * This is a low-level helper that updates the LED pin directly.
 * Prefer SetGreenLedMode for periodic blinking.
 */
void SetGreenLed(uint8_t state);

/**
 * @brief Configure LED blinking frequency and duty.
 * @param freq Frequency in Hz. If <= 0 the blinking is disabled and the
 *             LED is turned off.
 * @param duty Duty cycle as fraction 0.0 .. 1.0 OR as percent 0..100.
 *             The function normalizes values > 1.0 to percent semantics.
 *
 * Notes:
 *  - The function performs a short critical section (disables the timer
 *    IRQ) when updating shared state; it is fast and safe for common use.
 */
void SetGreenLedMode(float freq, float duty);
/* Backwards-compatible wrapper removed: call SetGreenLedMode directly. */
/* Convenience explicit-percent API
    SetGreenLedModePercent accepts duty as 0..100 (uint8_t percent) to avoid
    ambiguity between fraction (0..1) and percent. Internally it forwards
    to SetGreenLedMode which normalizes the value. */
/**
 * @brief Configure LED blinking using explicit percent duty.
 * @param freq Frequency in Hz.
 * @param duty_percent Duty as 0..100 (0 = off, 100 = full on).
 *
 * Convenience wrapper that forwards to SetGreenLedMode after normalizing
 * the duty value. Use this when callers want unambiguous percent units.
 */
void SetGreenLedModePercent(float freq, uint8_t duty_percent);
/* Inline wrapper for percent API as well. */
STATIC_INLINE void LedSetModePercent(float freq, uint8_t duty_percent) { SetGreenLedModePercent(freq, duty_percent); }

/* Convenience helpers forwarded to SetGreenLed; use STATIC_INLINE for internal linkage */
#ifndef STATIC_INLINE
#define STATIC_INLINE INLINE
#endif
STATIC_INLINE void SetGreenLedOn(void) { SetGreenLed(ON); }
STATIC_INLINE void SetGreenLedOff(void) { SetGreenLed(OFF); }

/**
 * @brief Millisecond tick counter (in LED_TIMER_TICK_MS units).
 *
 * This counter increments from the LED timer ISR and is useful for
 * lightweight timing in other modules. Reads are safe; writes are done
 * by the timer ISR.
 */
extern volatile uint32_t g_u32Ticks;

/**
 * @brief Blocking delay using the LED tick source.
 * @param u32DelayMs Delay in milliseconds (approximated using
 *                   LED_TIMER_TICK_MS if tick base != 1ms).
 */
void delay_ms(uint32_t u32DelayMs);

/**
 * @brief Read current tick count in milliseconds.
 * @return Millisecond tick value derived from g_u32Ticks / tick base.
 */
uint32_t get_ticks_ms(void);

/**
 * @brief Configure LED GPIO for SPD (power-down) mode.
 *
 * Called from power-management code to set the LED pin to the required
 * state for low-power operation.
 */
void Led_SpdModeGpio(void);

#endif // _LED_H_
