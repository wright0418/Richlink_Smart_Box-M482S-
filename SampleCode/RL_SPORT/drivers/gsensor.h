/**
 * @file gsensor.h
 * @brief G-sensor public API (SC7U22 + MXC400 auto-detect)
 *
 * This module auto-detects supported sensors on I2C at runtime and exposes
 * a unified API. Current supported devices:
 *   - SC7U22 (6-axis, accel + gyro)
 *   - MXC400  (3-axis accel)
 *
 * The API provides:
 *  - Initialization (I2C bus and sensor configuration)
 *  - Power management helpers (wake/power-down)
 *  - 3-axis / 6-axis raw read
 *  - Runtime information (active sensor type/name/address)
 *
 * Notes:
 *  - Caller must initialize via Gsensor_Init() before GsensorReadAxis().
 *  - Gsensor_Init() will auto-detect sensor backend and open/configure
 *    open/configure the I2C bus at the requested frequency and set the
 *    target full-scale range (FSR).
 *  - After entering power-down via GsensorPowerDown(), call GsensorWakeup()
 *    before attempting to read axis data.
 *  - The axis values returned by GsensorReadAxis() are raw sensor counts
 *    and require sensor-specific scaling when converting to g.
 */
#ifndef _GSENSOR_H_
#define _GSENSOR_H_

#include "../project_config.h"
#include <stdint.h>

typedef enum
{
    GSENSOR_DEVICE_NONE = 0,
    GSENSOR_DEVICE_SC7U22,
    GSENSOR_DEVICE_MXC400
} Gsensor_DeviceType;

/* Full-scale range enum for MXC400 G-sensor
   Values correspond to the sensor control register's FSR selection bits. */
typedef enum
{
    FSR_2G = 0,
    FSR_4G,
    FSR_8G
} Gsensor_FSR;

/**
 * @brief Get currently active sensor backend selected by auto-detection.
 * @return Active sensor type enum.
 */
Gsensor_DeviceType GsensorGetDeviceType(void);

/**
 * @brief Get human-readable active sensor name.
 * @return "SC7U22", "MXC400", or "NONE".
 */
const char *GsensorGetDeviceName(void);

/**
 * @brief Get active sensor I2C 7-bit address.
 * @return I2C address, or 0 if no sensor is active.
 */
uint8_t GsensorGetI2CAddress(void);

/**
 * @brief Put the G-sensor into low-power (power-down) mode.
 *
 * This function places the sensor into its low-power state. While in
 * power-down the sensor will not produce valid axis data until it is
 * woken up with GsensorWakeup().
 */
void GsensorPowerDown(void);

/**
 * @brief Wake the G-sensor and restore normal operation.
 *
 * After calling this API the sensor is returned to the configured full
 * scale range and is ready for axis readings. Allow any required sensor
 * internal startup delay before calling GsensorReadAxis().
 */
void GsensorWakeup(void);

/**
 * @brief Read X/Y/Z axis values from the G-sensor.
 * @param axis Pointer to an int16_t array with space for three elements.
 *             The function writes {X, Y, Z} raw sensor counts into the
 *             provided buffer.
 * @note Returned values are signed 12-bit left-aligned samples (stored in
 *       a int16_t). To convert to physical units (g) apply the sensitivity
 *       for the configured FSR.
 */
void GsensorReadAxis(int16_t *axis);

/**
 * @brief Read six-axis raw data from the selected sensor backend.
 * @param acc_axis Pointer to int16_t[3] for accelerometer {X,Y,Z}.
 * @param gyro_axis Pointer to int16_t[3] for gyroscope {X,Y,Z}.
 * @return 1 on success, 0 on communication/configuration failure.
 *
 * For 3-axis-only sensors such as MXC400, the accelerometer values are
 * returned normally and gyro values are filled with zero.
 */
uint8_t GsensorReadSixAxis(int16_t *acc_axis, int16_t *gyro_axis);

/**
 * @brief Read device-identification register or cached ID when available.
 * @param device_id Pointer receiving the current device ID value.
 * @return 1 if a valid ID was read, otherwise 0.
 */
uint8_t GsensorReadDeviceId(uint8_t *device_id);

/**
 * @brief Compute acceleration magnitude in g from raw axis counts.
 * @param axis Pointer to int16_t array with {X, Y, Z} raw samples.
 * @return Magnitude in units of g (floating point).
 *
 * This helper converts the raw 12-bit left-aligned counts returned by
 * GsensorReadAxis() into physical units using the currently configured
 * full-scale range (FSR) and returns the vector magnitude in g. The
 * function uses the module's configured FSR (set by GsensorInit()).
 */
float Gsensor_CalcMagnitude_g_from_raw(int16_t *axis);

/**
 * @brief Sensor-specific low-level power-down helper.
 *
 * Writes the sensor control register to place the device into power-down
 * while preserving the selected full-scale range bits. Typically used by
 * GsensorPowerDown(); exposed here for advanced usage or testing.
 */
void MXC400_to_PD(Gsensor_FSR fsr);

/**
 * @brief Sensor-specific wakeup helper.
 *
 * Restore sensor control register bits to bring the device back to normal
 * operating mode for the selected FSR. Typically used by GsensorWakeup().
 */
void MXC400_to_wakeup(Gsensor_FSR fsr);

/**
 * @brief Initialize G-sensor module and underlying I2C bus.
 * @param busHz I2C bus frequency in Hz (e.g. 100000 for 100kHz).
 * @param fsr   Desired full-scale range (FSR_2G / FSR_4G / FSR_8G).
 *
 * This function initializes I2C, auto-detects SC7U22/MXC400, and configures
 * the selected sensor backend. Caller should ensure system/module clocks are
 * enabled (typically done in SYS_Init()) before calling this function.
 */
void Gsensor_Init(uint32_t busHz, Gsensor_FSR fsr);

#endif // _GSENSOR_H_
