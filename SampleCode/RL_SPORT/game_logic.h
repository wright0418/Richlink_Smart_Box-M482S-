/**
 * @file game_logic.h
 * @brief Game/application state machine and logic module
 *
 * This module encapsulates the game state machine processing including:
 *  - Game start/stop handling
 *  - BLE connection state processing
 *  - Standby timeout management
 *  - Jump counting and data transmission
 */
#ifndef _GAME_LOGIC_H_
#define _GAME_LOGIC_H_

#include <stdint.h>

/* Standby timeout configuration (milliseconds) */
#define GAME_STANDBY_TIMEOUT (60 * 1000)  // 60 seconds

/**
 * @brief Initialize game logic module.
 *
 * Resets game state and initializes internal variables.
 * Call once during system startup.
 */
void Game_Init(void);

/**
 * @brief Process game logic when game is running (GAME_START state).
 *
 * Handles jump counting, G-sensor data (if enabled), and BLE data
 * transmission. Call repeatedly from main loop when game is active.
 */
void Game_ProcessRunning(void);

/**
 * @brief Process BLE disconnected state logic.
 *
 * Manages standby timeout and enters power-down mode if timeout expires.
 * Call repeatedly from main loop when BLE is disconnected.
 */
void Game_ProcessDisconnected(void);

/**
 * @brief Process game idle state (BLE connected but game stopped).
 *
 * Call from main loop when in GAME_STOP state.
 */
void Game_ProcessIdle(void);

/**
 * @brief Reset standby timer (call when BLE reconnects or activity occurs).
 */
void Game_ResetStandbyTimer(void);

#endif // _GAME_LOGIC_H_
