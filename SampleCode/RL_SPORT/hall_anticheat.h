/**
 * @file hall_anticheat.h
 * @brief Hall sensor anti-cheat validation using G-sensor + CMSIS DSP
 *
 * When USE_HALL_ANTICHEAT is enabled, this module runs the G-sensor in
 * parallel with the Hall sensor to estimate jump counts via FIR-filtered
 * peak detection. The Hall count is then validated against the G-sensor
 * estimate; if the Hall count diverges significantly (suggesting cheating
 * by manually rotating the magnet), excess counts are rejected.
 *
 * Mutually exclusive with USE_GSENSOR_JUMP_DETECT (which replaces Hall
 * entirely). Enable via project_config.h: #define USE_HALL_ANTICHEAT 1
 */
#ifndef _HALL_ANTICHEAT_H_
#define _HALL_ANTICHEAT_H_

#include <stdint.h>
#include "project_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if USE_HALL_ANTICHEAT

    /**
     * @brief Initialize the anti-cheat module.
     *
     * Sets up FIR filter, baseline collection state and peak detector.
     * Call once during system initialization.
     */
    void HallAntiCheat_Init(void);

    /**
     * @brief Feed one G-sensor sample for jump estimation.
     *
     * Call at ANTICHEAT_SAMPLE_RATE_HZ (default 50 Hz = every 20 ms).
     * During baseline collection the module accumulates statistics;
     * once the baseline is locked, peak detection counts jumps.
     *
     * @param axis Raw 3-axis accelerometer data [X, Y, Z] in sensor counts
     */
    void HallAntiCheat_Process(int16_t *axis);

    /**
     * @brief Reset jump counter (not baseline). Call on game start.
     *
     * Clears the G-sensor jump count and peak-detection state while
     * preserving the calibrated baseline so validation is immediate.
     */
    void HallAntiCheat_Reset(void);

    /**
     * @brief Get the G-sensor estimated jump count.
     * @return Estimated jump count since last reset
     */
    uint16_t HallAntiCheat_GetGsensorCount(void);

    /**
     * @brief Check whether baseline calibration is complete.
     * @return 1 if ready for validation, 0 if still collecting
     */
    uint8_t HallAntiCheat_IsReady(void);

    /**
     * @brief Validate a raw Hall total against G-sensor estimate.
     *
     * Returns min(hall_raw_total, gsensor_count + tolerance).
     * While baseline is not yet locked, returns hall_raw_total unchanged.
     *
     * @param hall_raw_total Cumulative raw Hall jump count
     * @return Validated (possibly capped) jump count
     */
    uint16_t HallAntiCheat_ValidateHallTotal(uint16_t hall_raw_total);

#endif /* USE_HALL_ANTICHEAT */

#ifdef __cplusplus
}
#endif

#endif /* _HALL_ANTICHEAT_H_ */
