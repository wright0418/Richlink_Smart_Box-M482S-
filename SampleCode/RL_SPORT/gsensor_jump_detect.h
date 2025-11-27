/**
 * @file gsensor_jump_detect.h
 * @brief G-Sensor based jump rope detection module
 * @version 1.0
 * @date 2025-11-28
 *
 * This module implements jump rope counting using 3-axis accelerometer data
 * with CMSIS DSP library for signal processing. It uses FIR low-pass filtering
 * and peak detection to identify jump events.
 *
 * Features:
 * - Button-triggered calibration (static baseline + dynamic threshold)
 * - FIR filter for noise reduction
 * - Peak detection with configurable threshold
 * - Buzzer feedback for calibration steps
 */

#ifndef _GSENSOR_JUMP_DETECT_H_
#define _GSENSOR_JUMP_DETECT_H_

#include <stdint.h>
#include "project_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if USE_GSENSOR_JUMP_DETECT

    /**
     * @brief Calibration states
     */
    typedef enum
    {
        CALIB_IDLE = 0,           /* No calibration in progress */
        CALIB_STATIC_COLLECTING,  /* Collecting static baseline data */
        CALIB_WAIT_DYNAMIC,       /* Waiting for user to start jumping */
        CALIB_DYNAMIC_COLLECTING, /* Collecting dynamic jump data */
        CALIB_COMPLETED,          /* Calibration successfully completed */
        CALIB_TIMEOUT,            /* Calibration timed out */
        CALIB_ERROR               /* Calibration error occurred */
    } CalibrationState;

    /**
     * @brief Calibration result data
     */
    typedef struct
    {
        float baseline_magnitude; /* Static baseline (gravity magnitude) */
        float dynamic_threshold;  /* Jump detection threshold */
        float peak_std_dev;       /* Standard deviation of peak magnitudes */
        uint8_t is_valid;         /* 1 if calibration data is valid */
    } CalibrationData;

    /**
     * @brief Initialize jump detection module
     *
     * Must be called once during system initialization.
     * Initializes FIR filter state and internal variables.
     */
    void JumpDetect_Init(void);

    /**
     * @brief Start calibration process
     *
     * Initiates the calibration sequence:
     * 1. Single beep - Place device in jump rope position and hold still
     * 2. After 2 seconds, double beep - Start jumping (8-10 jumps)
     * 3. After jumps detected, triple beep - Calibration complete
     *
     * @note This function is non-blocking. Call JumpDetect_ProcessCalibration()
     *       periodically to advance the calibration state machine.
     */
    void JumpDetect_StartCalibration(void);

    /**
     * @brief Process calibration state machine
     *
     * Should be called periodically (e.g., every 20ms) during calibration.
     * Advances through calibration states and collects required data.
     *
     * @param axis Raw 3-axis accelerometer data [X, Y, Z] in sensor counts
     * @return Current calibration state
     */
    CalibrationState JumpDetect_ProcessCalibration(int16_t *axis);

    /**
     * @brief Check if calibration is in progress
     *
     * @return 1 if calibration is active, 0 otherwise
     */
    uint8_t JumpDetect_IsCalibrating(void);

    /**
     * @brief Get current calibration state
     *
     * @return Current calibration state
     */
    CalibrationState JumpDetect_GetCalibrationState(void);

    /**
     * @brief Get calibration data
     *
     * @return Pointer to calibration data structure
     */
    const CalibrationData *JumpDetect_GetCalibrationData(void);

    /**
     * @brief Process G-sensor data for jump detection
     *
     * Call this function at regular intervals (50Hz recommended) with fresh
     * accelerometer data. It applies FIR filtering and peak detection to
     * identify jump events.
     *
     * When a jump is detected, it automatically calls Sys_IncrementJumpTimes().
     *
     * @param axis Raw 3-axis accelerometer data [X, Y, Z] in sensor counts
     * @note Calibration must be completed before jump detection will work
     */
    void JumpDetect_Process(int16_t *axis);

    /**
     * @brief Reset jump detection state
     *
     * Clears FIR filter state and peak detection variables.
     * Does not clear calibration data.
     */
    void JumpDetect_Reset(void);

    /**
     * @brief Get last calculated magnitude (for debugging)
     *
     * @return Last filtered acceleration magnitude
     */
    float JumpDetect_GetLastMagnitude(void);

    /**
     * @brief Check if module is ready for jump detection
     *
     * @return 1 if calibrated and ready, 0 otherwise
     */
    uint8_t JumpDetect_IsReady(void);

#endif /* USE_GSENSOR_JUMP_DETECT */

#ifdef __cplusplus
}
#endif

#endif /* _GSENSOR_JUMP_DETECT_H_ */
