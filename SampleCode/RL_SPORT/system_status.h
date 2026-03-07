/**
 * @file system_status.h
 * @brief System-wide status and state management module
 *
 * This module encapsulates all system state (BLE connection, game state,
 * button/sensor flags) and provides accessor APIs to avoid direct global
 * variable access from other modules.
 */
#ifndef _SYSTEM_STATUS_H_
#define _SYSTEM_STATUS_H_

#include <stdint.h>

/* BLE connection state */
typedef enum
{
    BLE_DISCONNECTED = 0,
    BLE_CONNECTED = 1
} BleState;

/* Game/application state */
typedef enum
{
    GAME_STOP = 0,
    GAME_START = 1
} GameState;

/* System status structure (encapsulates all global status/flags) */
typedef struct
{
    volatile BleState ble_state;
    volatile uint8_t ble_mode; // 0:CMD , 1:DATA
    volatile GameState game_state;
    volatile uint8_t keyA_state; // 0: Released , 1: Press
    volatile uint16_t jump_times;
    volatile uint16_t left_time_ms;
    volatile uint8_t mac_addr[24];
    volatile uint8_t device_name[30];
    volatile uint8_t keyA_flag;
    volatile uint8_t hall_pb7_irq_flag;
    volatile uint8_t hall_pb7_edge_pending;
    volatile uint8_t idle_state; /* 0: moving, 1: idle */
} SystemStatus;

/* Global instance (defined in system_status.c) */
extern SystemStatus g_sys;

/***************************************************************************
 * Accessor APIs (encapsulate state access to avoid direct g_sys manipulation)
 ***************************************************************************/

/* BLE state accessors */
static inline BleState Sys_GetBleState(void) { return g_sys.ble_state; }
static inline void Sys_SetBleState(BleState state) { g_sys.ble_state = state; }
static inline uint8_t Sys_GetBleMode(void) { return g_sys.ble_mode; }
static inline void Sys_SetBleMode(uint8_t mode) { g_sys.ble_mode = mode; }

/* Game state accessors */
static inline GameState Sys_GetGameState(void) { return g_sys.game_state; }
static inline void Sys_SetGameState(GameState state) { g_sys.game_state = state; }

/* Jump counter accessors */
static inline uint16_t Sys_GetJumpTimes(void) { return g_sys.jump_times; }
static inline void Sys_SetJumpTimes(uint16_t times) { g_sys.jump_times = times; }
static inline void Sys_IncrementJumpTimes(void) { g_sys.jump_times++; }
/**
 * @brief Atomically add jump count delta.
 * @param delta Number of jumps to add.
 */
void Sys_AddJumpTimes(uint16_t delta);
static inline void Sys_ResetJumpTimes(void) { g_sys.jump_times = 0; }

/* Button/sensor flag accessors */
static inline uint8_t Sys_GetKeyAFlag(void) { return g_sys.keyA_flag; }
static inline void Sys_SetKeyAFlag(uint8_t flag) { g_sys.keyA_flag = flag; }
static inline uint8_t Sys_GetHallPb7IrqFlag(void) { return g_sys.hall_pb7_irq_flag; }
static inline void Sys_SetHallPb7IrqFlag(uint8_t flag) { g_sys.hall_pb7_irq_flag = flag; }
/**
 * @brief Atomically accumulate one pending Hall PB7 edge event.
 *
 * Called by ISR only; saturates at 0xFF.
 */
void Sys_AccumulateHallPb7Edge(void);
/**
 * @brief Atomically take and clear pending Hall PB7 edge events.
 * @return Pending edge count accumulated since last take.
 */
uint8_t Sys_TakeHallPb7PendingEdges(void);
static inline uint8_t Sys_GetIdleState(void) { return g_sys.idle_state; }
static inline void Sys_SetIdleState(uint8_t state) { g_sys.idle_state = state; }

/* MAC address and device name accessors */
const char *Sys_GetMacAddr(void);
void Sys_SetMacAddr(const char *addr, uint32_t len);
const char *Sys_GetDeviceName(void);
void Sys_SetDeviceName(const char *name, uint32_t len);

/* Initialization */
void Sys_Init(void);

#endif // _SYSTEM_STATUS_H_
