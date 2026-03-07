/**
 * @file buzzer.h
 * @brief Simple buzzer control API
 *
 * Provides a minimal API to play tones on the board buzzer and to
 * integrate buzzer behavior with low-power modes (DLPS). The module
 * controls a single buzzer output pin (module-owned).
 */
#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>

/**
 * @brief Initialize buzzer hardware and internal state.
 *
 * Configures the buzzer GPIO and any required timer/PWM resources. Safe
 * to call multiple times; idempotent.
 */
void Buzzer_Init(void);

/**
 * @brief Play a tone for a specified duration.
 * @param freq Frequency in Hz.
 * @param time_ms Duration to play the tone in milliseconds.
 *
 * This function may block for the specified duration or schedule the
 * tone on a timer depending on the implementation. Use Buzzer_Start/
 * Buzzer_Stop for non-blocking control.
 */
void BuzzerPlay(uint32_t freq, uint32_t time_ms);

/**
 * @brief Start continuous tone playback at the given frequency.
 * @param freq Frequency in Hz.
 */
void Buzzer_Start(uint32_t freq);

/**
 * @brief Stop tone playback immediately.
 */
void Buzzer_Stop(void);

/**
 * @brief Configure buzzer GPIO for MCU DLPS (low-power) mode.
 *
 * Sets the buzzer pin to the state required by MCU DLPS to minimize
 * leakage or prevent unintended activity while the MCU is in low-power
 * mode. Called by power-management code when preparing to enter DLPS.
 */
void MCU_DLPS_GPIO(void);

/* Buzzer pin (module-owned) */
#define PIN_BUZZER PC7

/* Buzzer default parameters */
#define BUZZER_FREQ_DEFAULT 1000
#define BUZZER_TIME_DEFAULT 100

#endif // _BUZZER_H_
