/**
 * @file gsensor.h
 * @brief G-sensor (accelerometer) helper API
 *
 * Small wrapper API for the onboard G-sensor. Provides power management
 * helpers and an axis read function. The sensor is accessed over I2C; the
 * initialization function configures I2C and sensor pins.
 */
#ifndef _GSENSOR_H_
#define _GSENSOR_H_

#include <stdint.h>

#define GSENSOR_ADDR 0x15

/**
 * @brief Put G-sensor into low-power / power-down mode.
 */
void GsensorPowerDown(void);

/**
 * @brief Wake up and reconfigure the G-sensor.
 */
void GsensorWakeup(void);

/**
 * @brief Read X/Y/Z axis values from the G-sensor.
 * @param axis Pointer to a three-element int16_t array to receive {X,Y,Z}.
 */
void GsensorReadAxis(int16_t *axis);

/**
 * @brief Sensor-specific low-level power-down helper.
 */
void MXC400_to_PD(void);

/**
 * @brief Sensor-specific wakeup helper.
 */
void MXC400_to_wakeup(void);

/**
 * @brief Initialize I2C and configure sensor pins.
 * @param busHz I2C bus frequency in Hz used to communicate with sensor.
 */
void GsensorInit(uint32_t busHz);

#endif // _GSENSOR_H_
