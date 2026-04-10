/**
 * @file adc.h
 * @brief VDDA measurement via internal band-gap for low-voltage detection.
 */
#ifndef _ADC_H_
#define _ADC_H_

#include <stdint.h>

/**
 * @brief Initialize EADC for band-gap VDDA measurement.
 */
void Adc_Init(void);

/**
 * @brief Measure actual AVDD using internal band-gap (sample module 16) and
 *        cache the result. Call periodically to track AVDD.
 */
void Adc_UpdateVdda(void);

/**
 * @brief Return the last measured AVDD (V).
 * @return AVDD in volts.
 */
float Adc_GetVdda(void);

/**
 * @brief Check if AVDD is below the low-voltage threshold.
 * @return 1 if AVDD < ADC_VDDA_LOW_V, 0 otherwise.
 */
uint8_t Adc_IsVddaLow(void);

#endif // _ADC_H_
