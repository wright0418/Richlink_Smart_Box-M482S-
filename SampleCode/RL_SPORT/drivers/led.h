/**
 * @file led.h
 * @brief Green/Yellow LED control module (public API)
 *
 * This header exposes a small API for blinking and controlling the
 * GREEN LED on the board. It provides frequency/duty configuration
 * and lightweight timing helpers used by other modules.
 */
#ifndef _LED_H_
#define _LED_H_
#include "../project_config.h"
#include <stdint.h>

/* LED state shared variables (extern for observers/tests) */
extern volatile uint32_t g_u32OnTimeCounter;
extern volatile uint32_t g_u32PeriodCounter;
extern volatile float g_LedFreqHz;
extern volatile float g_LedDuty;

/* LED pin definition (module-owned) */
#define PIN_PIN_GREEN_LED PB3
#define PIN_PIN_YELLOW_LED PB2

/* LED default parameters */
#define LED_FREQ_DEFAULT 2
#define LED_DUTY_DEFAULT 50

/* Timer tick base used by the LED module (milliseconds per tick). */
#ifndef LED_TIMER_TICK_MS
#define LED_TIMER_TICK_MS 1
#endif

#define LED_TIMER_FREQ_HZ_INT (1000 / LED_TIMER_TICK_MS)
#define LED_TIMER_FREQ_HZ_F (1000.0f / (float)LED_TIMER_TICK_MS)

void Led_Init(void);
void SetGreenLed(uint8_t state);
void SetYellowLed(uint8_t state);
void SetGreenLedMode(float freq, float duty);
void SetGreenLedModePercent(float freq, uint8_t duty_percent);
void LedSetModePercent(float freq, uint8_t duty_percent);
void SetGreenLedOn(void);
void SetGreenLedOff(void);
extern volatile uint32_t g_u32Ticks;
void delay_ms(uint32_t u32DelayMs);
uint32_t get_ticks_ms(void);
void Led_SpdModeGpio(void);

#endif // _LED_H_
