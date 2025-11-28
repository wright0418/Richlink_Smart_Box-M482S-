/*
 * @file game_logic.c
 * @brief Game logic module implementation (clean single-definition)
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
#if USE_GSENSOR_JUMP_DETECT
#include "gsensor_jump_detect.h"
#endif

#include "movement.h"
#include "ble_helpers.h"
#include "powerdown.h"

/* Internal state */
static uint32_t standby_timer_start = 0;
static uint32_t last_ble_send_time = 0;

/* Helper: returns whether calibration visual/activity should inhibit
   normal LED/state changes. */
static inline uint8_t CalibrationActive(void)
{
#if USE_GSENSOR_JUMP_DETECT
    return JumpDetect_IsCalibrating();
#else
    (void)0;
    return 0;
#endif
}

/* Helper: set green LED mode unless calibration module needs to control LED */
static inline void SetGreenLedUnlessCalibrating(float freq_hz, float duty)
{
    if (!CalibrationActive())
        SetGreenLedMode(freq_hz, duty);
}

void Game_Init(void)
{
    Sys_SetGameState(GAME_STOP);
    Sys_ResetJumpTimes();

    standby_timer_start = get_ticks_ms();
    last_ble_send_time = 0;

    Movement_Init();
}

void Game_ResetStandbyTimer(void)
{
    standby_timer_start = get_ticks_ms();
}

void Game_ResetMovementTimer(void)
{
    Movement_Reset();
}

static void Game_CheckMovementAndMaybePowerDown(uint8_t ble_connected)
{
#if USE_GSENSOR_JUMP_DETECT
    if (JumpDetect_IsCalibrating())
        return;
#endif

    Movement_UpdateIfNeeded();
    uint32_t last_movement_time = Movement_GetLastMovementTime();

    uint32_t timeout_ms = ble_connected ? NO_MOVEMENT_TIMEOUT_CONNECTED_MS : NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS;
    if (is_timeout(last_movement_time, timeout_ms))
    {
        PowerDown_PerformSequence(ble_connected);
    }
}

void Game_ProcessRunning(void)
{
    SetGreenLedUnlessCalibrating(1, 0.1f);

#if enable_Gsensor_Mode
    int16_t axis[3];
    GsensorReadAxis(axis);
    DBG_PRINT("X,%d ,Y,%d ,Z,%d\n", axis[0], axis[1], axis[2]);
    BLE_SendSensorData(axis);
#else
    uint32_t now = get_ticks_ms();
    if (is_timeout(last_ble_send_time, BLE_SEND_INTERVAL_MS))
    {
        BLE_SendJumpCount();
        last_ble_send_time = now;
    }
#endif
}

void Game_ProcessIdle(void)
{
    SetGreenLedUnlessCalibrating(0.5f, 0.5f);
    Game_ResetStandbyTimer();
    Game_CheckMovementAndMaybePowerDown(1);
}

void Game_ProcessDisconnected(void)
{
    Sys_ResetJumpTimes();
    SetGreenLedUnlessCalibrating(0.5f, 0.5f);
    Game_CheckMovementAndMaybePowerDown(0);
}
