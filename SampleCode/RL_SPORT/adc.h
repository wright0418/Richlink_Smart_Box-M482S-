/**
 * @file adc.h
 * @brief Battery ADC helper (PB1 -> EADC0_CH1)
 */
#ifndef _ADC_H_
#define _ADC_H_

#include <stdint.h>

/**
 * @brief Initialize EADC for battery measurement.
 */
void Adc_InitBattery(void);

/**
 * @brief Read a single raw ADC sample.
 * @return 12-bit ADC raw value.
 */
uint16_t Adc_ReadBatteryRaw(void);

/**
 * @brief Read averaged raw ADC value.
 * @param samples Number of samples to average.
 * @return Averaged 12-bit ADC raw value.
 */
uint16_t Adc_ReadBatteryRawAvg(uint8_t samples);

/**
 * @brief Convert raw ADC value to battery voltage (V).
 * @param raw 12-bit ADC raw value.
 * @return Battery voltage in volts.
 */
float Adc_ConvertRawToBatteryV(uint16_t raw);

/**
 * @brief Determine if battery voltage is below threshold.
 * @param raw 12-bit ADC raw value.
 * @return 1 if low, 0 otherwise.
 */
uint8_t Adc_IsBatteryLow(uint16_t raw);

#endif // _ADC_H_
