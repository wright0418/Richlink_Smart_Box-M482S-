/**
 * @file game_logic.c
 * @brief Game logic module implementation
 */
#include "game_logic.h"
#include "system_status.h"
#include "timer.h"
#include "led.h"
#include "ble.h"
#include "gsensor.h"
#include "project_config.h"
#include <stdio.h>
#include <math.h>
#if USE_GSENSOR_JUMP_DETECT
#include "gsensor_jump_detect.h"
#endif

/* Internal state */
static uint32_t last_ble_send_time = 0;

/* Movement detection state */
static uint8_t movement_initialized = 0;
static uint32_t last_movement_time = 0;        /* last time movement was detected */
static uint32_t last_movement_sample_time = 0; /* last axis sample time */
static float movement_window[MOVEMENT_WINDOW_SAMPLES];
static uint8_t movement_window_idx = 0;
static uint8_t movement_window_count = 0;

void Game_Init(void)
{
    /* Reset game state */
    Sys_SetGameState(GAME_STOP);
    Sys_ResetJumpTimes();
    Sys_SetIdleState(0);

    /* Initialize movement detection */
    last_ble_send_time = 0;

    movement_initialized = 1;
    last_movement_time = get_ticks_ms();
    last_movement_sample_time = 0;
    movement_window_idx = 0;
    movement_window_count = 0;
    for (int i = 0; i < MOVEMENT_WINDOW_SAMPLES; i++)
        movement_window[i] = 0.0f;
}

void Game_ResetMovementTimer(void)
{
    /* Called when real activity or BLE reconnect happens */
    last_movement_time = get_ticks_ms();
    /* also clear movement window so short transients don't trigger */
    movement_window_idx = 0;
    movement_window_count = 0;
    for (int i = 0; i < MOVEMENT_WINDOW_SAMPLES; i++)
        movement_window[i] = 0.0f;
}

/* Internal helper: check movement and update idle state
    ble_connected: true when BLE is connected (use connected timeout), false otherwise */
static void Game_CheckMovementAndUpdateIdle(uint8_t ble_connected)
{
    uint32_t now = get_ticks_ms();

    /* If jump-detect calibration is running, do not consider movement
       inactivity for power-down. Calibration requires the sensor to be
       stationary or to collect baseline samples; disable PD detection
       while calibration is in progress. */
#if USE_GSENSOR_JUMP_DETECT
    if (JumpDetect_IsCalibrating())
        return;
#endif

    /* Sample at configured interval */
    if (!movement_initialized)
        return;

    if (!is_timeout(last_movement_sample_time, MOVEMENT_SAMPLE_INTERVAL_MS))
        return;

    last_movement_sample_time = now;

    int16_t axis[3];
    GsensorReadAxis(axis);
    float mag = Gsensor_CalcMagnitude_g_from_raw(axis);

    /* push into sliding window */
    movement_window[movement_window_idx] = mag;
    movement_window_idx = (movement_window_idx + 1) % MOVEMENT_WINDOW_SAMPLES;
    if (movement_window_count < MOVEMENT_WINDOW_SAMPLES)
        movement_window_count++;

    /* compute mean and stddev */
    float sum = 0.0f;
    for (uint8_t i = 0; i < movement_window_count; i++)
        sum += movement_window[i];
    float mean = sum / (float)movement_window_count;

    float var = 0.0f;
    for (uint8_t i = 0; i < movement_window_count; i++)
    {
        float d = movement_window[i] - mean;
        var += d * d;
    }
    float stddev = 0.0f;
    if (movement_window_count > 0)
        stddev = sqrtf(var / (float)movement_window_count);

    /* Detect movement using shared algorithm helper for unit-test consistency */
    uint8_t is_moving = GameAlgo_IsMovement(stddev, mean, MOVEMENT_STDDEV_THRESHOLD_G, MOVEMENT_MAG_TOLERANCE_G);

#if MOVEMENT_DEBUG
    DBG_PRINT("[MOV] raw=%d,%d,%d mag=%.3f mean=%.3f std=%.4f | %s | idle=%d elapsed=%lums\n",
              axis[0], axis[1], axis[2],
              (double)mag, (double)mean, (double)stddev,
              is_moving ? "MOVING" : "STILL",
              Sys_GetIdleState(),
              (unsigned long)(now - last_movement_time));
#endif

    if (is_moving)
    {
        /* movement detected -> reset last movement time */
        last_movement_time = now;
        if (Sys_GetIdleState())
        {
            Sys_SetIdleState(0);
            DBG_PRINT("[Game] Movement resumed\n");
        }
        return;
    }

    /* No significant movement detected; check timeout depending on BLE state */
    uint32_t timeout_ms = ble_connected ? NO_MOVEMENT_TIMEOUT_CONNECTED_MS : NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS;
    if (is_timeout(last_movement_time, timeout_ms))
    {
        if (!Sys_GetIdleState())
        {
            Sys_SetIdleState(1);
            DBG_PRINT("[Game] Idle detected (no movement)\n");
        }
        return;
    }
}

void Game_ProcessRunning(void)
{
    /* Safety: Verify we are still in valid state (BLE connected + game running) */
    if (Sys_GetBleState() != BLE_CONNECTED || Sys_GetGameState() != GAME_START)
    {
        return;
    }

    /* Send jump count periodically (every 100ms) */
    uint32_t now = get_ticks_ms();
    if (is_timeout(last_ble_send_time, 100))
    {
        uint16_t total = Sys_GetJumpTimes();
        char bleData[32];
        snprintf(bleData, sizeof(bleData), "send,%u\n", (unsigned)total);
        DBG_PRINT("[BLE] send total=%u\n", (unsigned)total);
        BLESendData(bleData);
        last_ble_send_time = now;
    }
}

void Game_ResetBleTimer(void)
{
    /* Clear BLE send timeout so next send happens immediately on reconnect */
    last_ble_send_time = 0;
}

void Game_ProcessIdle(void)
{
    /* Check movement and update idle state (BLE connected case) */
    Game_CheckMovementAndUpdateIdle(1);
}

void Game_ProcessDisconnected(void)
{
    /* Reset game state */
    Sys_ResetJumpTimes();

    /* Check movement and update idle state (BLE disconnected case) */
    Game_CheckMovementAndUpdateIdle(0);
}
