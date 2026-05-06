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

#define SYS_MAC_ADDR_BUF_SIZE 24u
#define SYS_DEVICE_NAME_BUF_SIZE 30u

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
    volatile uint16_t jump_times;
    volatile uint8_t mac_addr[SYS_MAC_ADDR_BUF_SIZE];
    volatile uint8_t device_name[SYS_DEVICE_NAME_BUF_SIZE];
    volatile uint8_t keyA_flag;
    volatile uint8_t hall_pb7_irq_flag;
    volatile uint8_t hall_pb7_edge_pending;
    volatile uint8_t idle_state;        /* 0: moving, 1: idle */
    volatile uint8_t repl_mode;         /* 0: normal game mode, 1: BLE REPL test mode */
    volatile uint8_t repl_led_override; /* 0: main LED allowed, 1: REPL/override controls LED */
} SystemStatus;

/***************************************************************************
 * Accessor APIs (encapsulate state access to avoid direct g_sys manipulation)
 ***************************************************************************/

/* BLE state accessors */
BleState Sys_GetBleState(void);
void Sys_SetBleState(BleState state);
uint8_t Sys_GetBleMode(void);
void Sys_SetBleMode(uint8_t mode);

/* Game state accessors */
GameState Sys_GetGameState(void);
void Sys_SetGameState(GameState state);

/* Jump counter accessors */
uint16_t Sys_GetJumpTimes(void);
void Sys_SetJumpTimes(uint16_t times);
/**
 * @brief Atomically add jump count delta.
 * @param delta Number of jumps to add.
 */
void Sys_AddJumpTimes(uint16_t delta);
void Sys_IncrementJumpTimes(void);
void Sys_ResetJumpTimes(void);

/* Button/sensor flag accessors */
uint8_t Sys_GetKeyAFlag(void);
void Sys_SetKeyAFlag(uint8_t flag);
uint8_t Sys_GetHallPb7IrqFlag(void);
void Sys_SetHallPb7IrqFlag(uint8_t flag);
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
uint8_t Sys_GetIdleState(void);
void Sys_SetIdleState(uint8_t state);
uint8_t Sys_GetReplMode(void);
void Sys_SetReplMode(uint8_t mode);

/* LED override accessors: when set, main LED update should skip reapplying blink */
uint8_t Sys_GetLedOverride(void);
void Sys_SetLedOverride(uint8_t v);

/* MAC address and device name accessors */
void Sys_SetMacAddr(const char *addr, uint32_t len);
uint32_t Sys_CopyMacAddr(char *dst, uint32_t dst_size);
void Sys_ClearMacAddr(void);
void Sys_SetDeviceName(const char *name, uint32_t len);
uint32_t Sys_CopyDeviceName(char *dst, uint32_t dst_size);
void Sys_ClearDeviceName(void);

/* Initialization */
void Sys_Init(void);

#endif // _SYSTEM_STATUS_H_
