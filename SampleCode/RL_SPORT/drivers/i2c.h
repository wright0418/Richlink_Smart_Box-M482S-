/**
 * @file drivers/i2c.h
<<<<<<< HEAD
 * @brief Simple I2C wrapper used by board modules (G-sensor etc.)
 *
 * NOTE: moved from top-level RL_SPORT directory into drivers/.
 * API and behavior remain unchanged.
=======
 * @brief Simple and robust I2C wrapper used by board drivers.
>>>>>>> 增加-6-axis-sensor-SC7U22
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
<<<<<<< HEAD
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
 * This wrapper performs up to I2C_XFER_RETRY_COUNT retries when transfer
 * fails (including timeout-flag recovery).
=======
 * @brief I2C write: 1-byte register address + 1-byte data.
>>>>>>> 增加-6-axis-sensor-SC7U22
 * @return 0 on success; non-zero on failure after retries.
 */
uint8_t RL_I2C_WriteByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t data);

/**
<<<<<<< HEAD
 * @brief I2C read: 1-byte register address + multi-byte read.
 *
 * This wrapper performs up to I2C_XFER_RETRY_COUNT retries when read length
 * is shorter than requested.
=======
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
>>>>>>> 增加-6-axis-sensor-SC7U22
 * @return Number of bytes read. Expected value is u32rLen.
 */
uint32_t RL_I2C_ReadMultiBytesOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t rdata[], uint32_t u32rLen);

#endif // _I2C_WRAPPER_H_
