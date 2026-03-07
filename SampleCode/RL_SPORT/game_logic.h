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
#include <math.h>

/**
 * @brief Determine whether current statistics indicate movement.
 * @param stddev_g Standard deviation of acceleration magnitude window (g).
 * @param mean_mag_g Mean acceleration magnitude of window (g).
 * @param stddev_threshold_g Movement stddev threshold (g).
 * @param mag_tolerance_g Allowed absolute deviation from 1g.
 * @return 1 if movement detected, 0 if considered stationary.
 *
 * Decision rule:
 * - movement when stddev_g > stddev_threshold_g, OR
 * - movement when |mean_mag_g - 1.0| > mag_tolerance_g.
 */
static inline uint8_t GameAlgo_IsMovement(float stddev_g,
                                          float mean_mag_g,
                                          float stddev_threshold_g,
                                          float mag_tolerance_g)
{
    return (uint8_t)((stddev_g > stddev_threshold_g) ||
                     (fabsf(mean_mag_g - 1.0f) > mag_tolerance_g));
}

/**
 * @brief Convert Hall falling-edge counts to jump count with residual support.
 * @param residual_edges_in Residual edges from previous cycle (typically 0 or 1).
 * @param new_edges Newly captured edges in current cycle.
 * @param residual_edges_out Output residual edges for next cycle.
 * @return Number of complete jumps calculated in this cycle.
 *
 * Counting rule: 2 falling edges = 1 jump.
 * Error handling:
 * - If residual_edges_out is NULL, function returns 0 and performs no update.
 */
static inline uint16_t GameAlgo_CalcJumpsFromEdges(uint8_t residual_edges_in,
                                                   uint8_t new_edges,
                                                   uint8_t *residual_edges_out)
{
    if (!residual_edges_out)
    {
        return 0u;
    }

    uint16_t total_edges = (uint16_t)residual_edges_in + (uint16_t)new_edges;
    uint16_t jumps = (uint16_t)(total_edges / 2u);
    *residual_edges_out = (uint8_t)(total_edges % 2u);
    return jumps;
}

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
