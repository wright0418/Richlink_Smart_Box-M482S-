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

/* Enable G-sensor mode (0=disabled, 1=enabled) */
#define enable_Gsensor_Mode 0

/* Internal state */
static uint32_t standby_timer_start = 0;
static uint32_t last_ble_send_time = 0;

void Game_Init(void)
{
    /* Reset game state */
    Sys_SetGameState(GAME_STOP);
    Sys_ResetJumpTimes();

    /* Initialize standby timer */
    standby_timer_start = get_ticks_ms();
    last_ble_send_time = 0;
}

void Game_ResetStandbyTimer(void)
{
    standby_timer_start = get_ticks_ms();
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
}

void Game_ProcessDisconnected(void)
{
    /* Reset game state */
    Sys_ResetJumpTimes();

    /* Set LED to slow blink (0.5Hz, 50% duty) */
    SetGreenLedMode(0.5, 0.5);

    /* Check standby timeout */
    if (is_timeout(standby_timer_start, GAME_STANDBY_TIMEOUT))
    {
        /* Timeout expired: prepare for power-down */
        SetGreenLedMode(1, 0); // Turn off LED
        DBG_PRINT("Enter to Power-Down ......\n");

        /* Put G-sensor to power-down */
        GsensorPowerDown();

        /* Disconnect BLE (enter DLPS mode) */
        BLE_DISCONNECT();

        /* Play notification beep */
        BuzzerPlay(2000, 100);

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
