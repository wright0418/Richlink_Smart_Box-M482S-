/**
 * @file power_mgmt.h
 * @brief Power management module for Deep/Shallow Power-Down modes
 *
 * This module centralizes all power management functionality including:
 *  - Deep Power-Down (DPD) mode configuration and wake-up
 *  - Shallow Power-Down (SPD) mode configuration and wake-up
 *  - GPIO configuration for low-power modes
 *  - Wake-up pin configuration
 */
#ifndef _POWER_MGMT_H_
#define _POWER_MGMT_H_

#include <stdint.h>

/* Power-down mode selection */
typedef enum
{
    PWR_MODE_DPD,  /* Deep Power-Down */
    PWR_MODE_SPD0, /* Shallow Power-Down mode 0 */
    PWR_MODE_SPD1  /* Shallow Power-Down mode 1 */
} PowerMode;

/* Wake-up edge type for DPD mode */
typedef enum
{
    PWR_WAKEUP_RISING = 0,
    PWR_WAKEUP_FALLING = 1
} WakeupEdge;

/**
 * @brief Enter Deep Power-Down mode with PC0 wake-up.
 * @param edge Wake-up edge type (rising or falling).
 *
 * Configures PC0 as DPD wake-up pin and enters DPD mode.
 * System will reset upon wake-up. This function does not return.
 */
void PowerMgmt_EnterDPD(WakeupEdge edge);

/**
 * @brief Enter Shallow Power-Down mode with PB15 wake-up.
 * @param mode SPD mode selection (SPD0 or SPD1).
 *
 * Configures PB15 (KeyA) as SPD wake-up pin and enters SPD mode.
 * System will reset upon wake-up. This function does not return.
 */
void PowerMgmt_EnterSPD(PowerMode mode);

/**
 * @brief Configure GPIO pins for SPD mode (set as input to reduce leakage).
 *
 * Called before entering SPD to minimize power consumption.
 */
void PowerMgmt_ConfigGpioForSPD(void);

/**
 * @brief Release I/O hold status after wake-up from SPD mode.
 *
 * Should be called early in initialization after SPD wake-up.
 */
void PowerMgmt_ReleaseIOHold(void);

/**
 * @brief Handle wake-up operations after returning from SPD.
 *
 * This function checks PMU wake flags, prints diagnostics, releases I/O hold,
 * clears wake flags and reconfigures wake-related GPIOs (e.g. PB15) as needed.
 * It is safe to call early during system initialization after SYS_Init().
 */
void PowerMgmt_HandleWake(void);

#endif // _POWER_MGMT_H_
