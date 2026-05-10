/**
 * @file drivers/i2c.h
 * @brief Simple and robust I2C wrapper used by board drivers.
 */
#ifndef _I2C_WRAPPER_H_
#define _I2C_WRAPPER_H_

#include "NuMicro.h"
#include <stdint.h>

/**
 * @brief Initialize I2C peripheral (I2C0).
 * @param u32BusHz Bus frequency in Hz.
 */
void I2C_Init(uint32_t u32BusHz);

/**
 * @brief I2C write: 1-byte register address + 1-byte data.
 * @return 0 on success; non-zero on failure after retries.
 */
uint8_t RL_I2C_WriteByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t data);

/**
 * @brief Probe whether an I2C 7-bit address ACKs SLA+W.
 * @return 1 if address ACKed; 0 otherwise.
 */
uint8_t RL_I2C_ProbeAddress(I2C_T *i2c, uint8_t u8SlaveAddr);

/**
 * @brief I2C read: 1-byte register address + 1-byte read.
 * @return 0 on success; non-zero on failure after retries.
 */
uint8_t RL_I2C_ReadByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t *data);

/**
 * @brief I2C read: 1-byte register address + multi-byte read.
 * @return Number of bytes read. Expected value is u32rLen.
 */
uint32_t RL_I2C_ReadMultiBytesOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t rdata[], uint32_t u32rLen);

#endif // _I2C_WRAPPER_H_
