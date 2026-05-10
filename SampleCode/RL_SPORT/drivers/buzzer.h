/**
 * @file buzzer.h
 * @brief Simple buzzer control API
 */
#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>

void Buzzer_Init(void);
void BuzzerPlay(uint32_t freq, uint32_t time_ms);
void Buzzer_Start(uint32_t freq);
void Buzzer_Stop(void);
void MCU_DLPS_GPIO(void);

#define PIN_BUZZER PC7
#define BUZZER_FREQ_DEFAULT 1000
#define BUZZER_TIME_DEFAULT 100

#endif // _BUZZER_H_
