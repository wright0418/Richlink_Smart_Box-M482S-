/**
 * @file gsensor.h
 * @brief G-sensor (accelerometer) public API
 *
 * This header exposes a small, portable API for the on-board MXC400-series
 * accelerometer used by the RL_SPORT sample. The module provides:
 *  - Initialization (I2C bus and sensor configuration)
 *  - Power management helpers (wake/power-down)
 *  - A simple axis read API returning signed axis values in raw sensor
 *    counts (12-bit left-aligned data; see notes below)
 *
 * Notes:
 *  - Caller must initialize the I2C peripheral and sensor via
 *    GsensorInit() before calling GsensorReadAxis(). GsensorInit() will
 *    open/configure the I2C bus at the requested frequency and set the
 *    sensor FSR.
 *  - After entering power-down via GsensorPowerDown(), call GsensorWakeup()
 *    before attempting to read axis data.
 *  - The axis values returned by GsensorReadAxis() are raw sensor counts
 *    (12-bit data packed into a signed int16). Conversion to physical
 *    units (e.g., g) requires applying the chosen full-scale range (FSR)
 *    and the sensor's sensitivity; this header intentionally leaves unit
 *    conversion to the caller to keep the API simple.
 */
#ifndef _GSENSOR_H_
#define _GSENSOR_H_

#include <stdint.h>

#define GSENSOR_ADDR 0x15

/* Full-scale range enum for MXC400 G-sensor
   Values correspond to the sensor control register's FSR selection bits. */
typedef enum
{
    FSR_2G = 0,
    FSR_4G,
    FSR_8G
} Gsensor_FSR;

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
 * This function initializes the I2C peripheral used to communicate with
 * the MXC400 accelerometer and programs the sensor's full-scale range.
 * The caller should ensure system clocks and module clocks are enabled
 * (typically done in SYS_Init()) before calling this function.
 */
void GsensorInit(uint32_t busHz, Gsensor_FSR fsr);

#endif // _GSENSOR_H_
