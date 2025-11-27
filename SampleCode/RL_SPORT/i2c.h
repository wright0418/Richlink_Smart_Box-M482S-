/**
 * @file i2c.h
 * @brief Simple I2C wrapper used by board modules (G-sensor etc.)
 */
#ifndef _I2C_WRAPPER_H_
#define _I2C_WRAPPER_H_

#include <stdint.h>

/**
 * @brief Initialize I2C peripheral (I2C0).
 * @param u32BusHz Bus frequency in Hz.
 */
void I2C_Init(uint32_t u32BusHz);

#endif // _I2C_WRAPPER_H_
