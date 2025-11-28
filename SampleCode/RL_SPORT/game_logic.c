/**
 * @file game_logic.c
 * @brief Game logic module implementation
 */
#include "game_logic.h"
#include "system_status.h"
#include "timer.h"
#include "led.h"
#include "buzzer.h"
#include "ble.h"
#include "gsensor.h"
#include "power_mgmt.h"
#include "project_config.h"
#include <stdio.h>
#include <math.h>
#if USE_GSENSOR_JUMP_DETECT
#include "gsensor_jump_detect.h"
#endif

/* Enable G-sensor mode (0=disabled, 1=enabled) */
#define enable_Gsensor_Mode 0

/* Internal state */
static uint32_t standby_timer_start = 0;
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

    /* Initialize standby timer and movement detection */
    standby_timer_start = get_ticks_ms();
    last_ble_send_time = 0;

    movement_initialized = 1;
    last_movement_time = get_ticks_ms();
    last_movement_sample_time = 0;
    movement_window_idx = 0;
    movement_window_count = 0;
    for (int i = 0; i < MOVEMENT_WINDOW_SAMPLES; i++)
        movement_window[i] = 0.0f;
}

void Game_ResetStandbyTimer(void)
{
    /* Reset only the standby timer. Do NOT reset movement timer here —
       Game_ProcessIdle is called repeatedly while BLE is connected and
       resetting last_movement_time here would prevent movement-based
       power-down from ever triggering. Use a dedicated movement reset
       when actual user activity or BLE reconnect occurs. */
    standby_timer_start = get_ticks_ms();
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

/* Internal helper: check movement and enter power-down when appropriate
   ble_connected: true when BLE is connected (use connected timeout), false otherwise */
static void Game_CheckMovementAndMaybePowerDown(uint8_t ble_connected)
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

    /* Detect movement: either stddev large or magnitude deviates from 1g */
    if (stddev > MOVEMENT_STDDEV_THRESHOLD_G || fabsf(mean - 1.0f) > MOVEMENT_MAG_TOLERANCE_G)
    {
        /* movement detected -> reset last movement time */
        last_movement_time = now;
        return;
    }

    /* No significant movement detected; check timeout depending on BLE state */
    uint32_t timeout_ms = ble_connected ? NO_MOVEMENT_TIMEOUT_CONNECTED_MS : NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS;
    if (is_timeout(last_movement_time, timeout_ms))
    {
        /* Timeout expired: prepare for power-down */
        SetGreenLedMode(1, 0); // Turn off LED
        DBG_PRINT("Enter to Power-Down (no movement) ......\n");

        /* Put G-sensor to power-down */
        GsensorPowerDown();

        /* If BLE is connected, disconnect first and wait briefly for the
           module to acknowledge disconnect so the link is clean before
           power-down. */
        if (ble_connected)
        {
            BLE_DISCONNECT();
            /* Wait up to 1 second for BLE state to become DISCONNECTED */
            uint32_t wait_start = get_ticks_ms();
            while (!is_timeout(wait_start, 1000))
            {
                if (Sys_GetBleState() == BLE_DISCONNECTED)
                    break;
                delay_ms(50);
            }
        }

        BLE_to_DLPS();
        delay_ms(100);
        /* Play notification beep (3 short beeps at 4000 Hz) */
        BuzzerPlay(4000, 100);
        delay_ms(500); /* Wait for beep to finish + gap */
        BuzzerPlay(4000, 100);
        delay_ms(500);
        BuzzerPlay(4000, 100);
        delay_ms(500);

        /* Enter power-down mode (DPD or SPD based on board config) */
#ifdef DPD_PC0
        PowerMgmt_EnterDPD(PWR_WAKEUP_RISING);
#else
        PowerMgmt_EnterSPD(PWR_MODE_SPD0);
#endif

        /* Should not reach here (system resets on wake-up) */
        while (1)
        {
        }
    }
}

void Game_ProcessRunning(void)
{
    /* Set LED to fast blink (1Hz, 10% duty) during game */
    SetGreenLedMode(1, 0.1);

#if enable_Gsensor_Mode
    /* G-sensor mode: read axis data and send to BLE */
    int16_t axis[3];
    GsensorReadAxis(axis);
    DBG_PRINT("X,%d ,Y,%d ,Z,%d\n", axis[0], axis[1], axis[2]);

    char bleData[64];
    snprintf(bleData, sizeof(bleData), "send,%d,%d,%d,%d\n",
             Sys_GetJumpTimes() >> 1, axis[0], axis[1], axis[2]);
    BLESendData(bleData);
#else
    /* Normal mode: send jump count periodically (every 100ms) */
    uint32_t now = get_ticks_ms();
    if (is_timeout(last_ble_send_time, 100))
    {
        char bleData[32];
        snprintf(bleData, sizeof(bleData), "send,%d\n", Sys_GetJumpTimes());
        BLESendData(bleData);
        last_ble_send_time = now;
    }
#endif
}

void Game_ProcessIdle(void)
{
    /* BLE connected but game stopped: slow LED blink (0.5Hz, 50% duty) */
    SetGreenLedMode(0.5, 0.5);
    /* Reset standby timer when BLE is connected */
    Game_ResetStandbyTimer();

    /* Check movement and possibly enter power-down (BLE connected case) */
    Game_CheckMovementAndMaybePowerDown(1);
}

void Game_ProcessDisconnected(void)
{
    /* Reset game state */
    Sys_ResetJumpTimes();

    /* Set LED to slow blink (0.5Hz, 50% duty) */
    SetGreenLedMode(0.5, 0.5);

    /* Check movement and possibly enter power-down (BLE disconnected case) */
    Game_CheckMovementAndMaybePowerDown(0);
}
