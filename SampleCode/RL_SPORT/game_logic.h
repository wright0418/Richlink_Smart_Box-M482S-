/**
 * @file game_logic.h
 * @brief Game/application state machine and logic module
 *
 * This module encapsulates the game state machine processing including:
 *  - Game start/stop handling
 *  - BLE connection state processing
 *  - Movement/idle detection
 *  - Jump counting and data transmission
 */
#ifndef _GAME_LOGIC_H_
#define _GAME_LOGIC_H_

#include <stdint.h>

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
 * Updates movement/idle detection state while BLE is disconnected.
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
 * @brief Reset movement inactivity timer.
 *
 * Call this when a real user activity or a BLE reconnect occurs so that the
 * movement-based idle timer is restarted. This should NOT be called from
 * the periodic idle path.
 */
void Game_ResetMovementTimer(void);

/**
 * @brief Reset BLE periodic send timer.
 *
 * Called on BLE reconnect to clear the timeout reference, ensuring the next
 * send happens immediately rather than waiting for residual delay.
 */
void Game_ResetBleTimer(void);

#endif // _GAME_LOGIC_H_
