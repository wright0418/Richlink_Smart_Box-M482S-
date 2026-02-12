/**
 * @file i2c.h
 * @brief Simple I2C wrapper used by board modules (G-sensor etc.)
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
 * @brief Enable/disable RL_SPORT I2C debug logs.
 *
 * When disabled (default), RL_I2C_* wrappers will not print any status.
 * Board tests can enable this temporarily.
 */
void RL_I2C_SetDebugLog(uint8_t enable);
uint8_t RL_I2C_GetDebugLog(void);

/**
 * @brief I2C write: 1-byte register address + 1-byte data.
 *
 * This is a thin wrapper around StdDriver I2C_WriteByteOneReg() that prints
 * the return status, I2C STATUS code, and StdDriver global error code when
 * an error/abnormal condition is detected.
 */
uint8_t RL_I2C_WriteByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t data);

/**
 * @brief I2C read: 1-byte register address + multi-byte read.
 *
 * This is a thin wrapper around StdDriver I2C_ReadMultiBytesOneReg() that prints
 * the return status, I2C STATUS code, and StdDriver global error code when
 * an error/abnormal condition is detected.
 */
uint32_t RL_I2C_ReadMultiBytesOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t rdata[], uint32_t u32rLen);

#endif // _I2C_WRAPPER_H_
