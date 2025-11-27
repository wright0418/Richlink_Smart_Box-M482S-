/* buzzer.h - Buzzer control module */
#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>

void Buzzer_Init(void);
void BuzzerPlay(uint32_t freq, uint32_t time_ms);
void Buzzer_Start(uint32_t freq);
void Buzzer_Stop(void);
void MCU_DLPS_GPIO(void);

/* Buzzer pin (module-owned) */
#define PIN_BUZZER PC7

/* Buzzer default parameters */
#define BUZZER_FREQ_DEFAULT 1000
#define BUZZER_TIME_DEFAULT 100

#endif // _BUZZER_H_
