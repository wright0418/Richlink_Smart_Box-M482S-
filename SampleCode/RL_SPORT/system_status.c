/**
 * @file system_status.c
 * @brief System-wide status and state management implementation.
 *
 * This module centralizes all application-level flags and runtime state.
 * Accessor functions are provided so other modules do not manipulate the
 * underlying global state directly. IRQ-safe state writes use __disable_irq()
 * / __enable_irq() to avoid tearing in the shared `SystemStatus` structure.
 */
#include "system_status.h"
#include "NuMicro.h"
#include <string.h>

/* Global system status instance kept private to this translation unit. */
static SystemStatus g_sys;

/**
 * @brief Copy a string into a volatile destination buffer.
 * @param dst Destination buffer.
 * @param dst_size Size of the destination buffer including NUL.
 * @param src Source string.
 * @param len Number of characters to copy from src.
 */
static void Sys_SetString(volatile uint8_t *dst, uint32_t dst_size, const char *src, uint32_t len)
{
    if (!dst || !src || dst_size == 0u || len >= dst_size)
    {
        return;
    }

    __disable_irq();
    if (len == 0u)
    {
        dst[0] = '\0';
        __enable_irq();
        return;
    }

    memcpy((void *)dst, src, len);
    dst[len] = '\0';
    __enable_irq();
}

/**
 * @brief Copy a volatile string into a normal char buffer safely.
 * @param src Source volatile string buffer.
 * @param src_size Size of the source buffer.
 * @param dst Destination buffer.
 * @param dst_size Capacity of the destination buffer, including NUL.
 * @return Number of characters copied (excluding final NUL).
 */
static uint32_t Sys_CopyString(const volatile uint8_t *src, uint32_t src_size, char *dst, uint32_t dst_size)
{
    uint32_t i = 0u;

    if (!src || !dst || dst_size == 0u)
    {
        return 0u;
    }

    __disable_irq();
    while (i < src_size && (i + 1u) < dst_size)
    {
        uint8_t ch = src[i];
        dst[i] = (char)ch;
        if (ch == '\0')
        {
            __enable_irq();
            return i;
        }
        i++;
    }
    __enable_irq();

    dst[i] = '\0';
    return i;
}

BleState Sys_GetBleState(void)
{
    return g_sys.ble_state;
}

void Sys_SetBleState(BleState state)
{
    g_sys.ble_state = state;
}

uint8_t Sys_GetBleMode(void)
{
    return g_sys.ble_mode;
}

void Sys_SetBleMode(uint8_t mode)
{
    g_sys.ble_mode = mode;
}

GameState Sys_GetGameState(void)
{
    return g_sys.game_state;
}

void Sys_SetGameState(GameState state)
{
    g_sys.game_state = state;
}

uint16_t Sys_GetJumpTimes(void)
{
    return g_sys.jump_times;
}

void Sys_IncrementJumpTimes(void)
{
    Sys_AddJumpTimes(1u);
}

void Sys_ResetJumpTimes(void)
{
    Sys_SetJumpTimes(0u);
}

uint8_t Sys_GetKeyAFlag(void)
{
    return g_sys.keyA_flag;
}

void Sys_SetKeyAFlag(uint8_t flag)
{
    g_sys.keyA_flag = flag;
}

uint8_t Sys_GetHallPb7IrqFlag(void)
{
    return g_sys.hall_pb7_irq_flag;
}

void Sys_SetHallPb7IrqFlag(uint8_t flag)
{
    g_sys.hall_pb7_irq_flag = flag;
}

uint8_t Sys_GetIdleState(void)
{
    return g_sys.idle_state;
}

void Sys_SetIdleState(uint8_t state)
{
    g_sys.idle_state = state;
}

uint8_t Sys_GetReplMode(void)
{
    return g_sys.repl_mode;
}

void Sys_SetReplMode(uint8_t mode)
{
    g_sys.repl_mode = mode;
}

uint8_t Sys_GetLedOverride(void)
{
    return g_sys.repl_led_override;
}

void Sys_SetLedOverride(uint8_t v)
{
    g_sys.repl_led_override = v;
}

void Sys_Init(void)
{
    /* Initialize all system status fields to default values */
    g_sys.ble_state = BLE_DISCONNECTED;
    g_sys.ble_mode = 0;
    g_sys.game_state = GAME_STOP;
    g_sys.jump_times = 0;
    memset((void *)g_sys.mac_addr, 0, sizeof(g_sys.mac_addr));
    memset((void *)g_sys.device_name, 0, sizeof(g_sys.device_name));
    g_sys.keyA_flag = 0;
    g_sys.hall_pb7_irq_flag = 0;
    g_sys.hall_pb7_edge_pending = 0;
    g_sys.idle_state = 0;
    g_sys.repl_mode = 0;
    g_sys.repl_led_override = 0;
}

void Sys_AddJumpTimes(uint16_t delta)
{
    __disable_irq();
    g_sys.jump_times = (uint16_t)(g_sys.jump_times + delta);
    __enable_irq();
}

void Sys_SetJumpTimes(uint16_t times)
{
    __disable_irq();
    g_sys.jump_times = times;
    __enable_irq();
}

void Sys_AccumulateHallPb7Edge(void)
{
    __disable_irq();
    if (g_sys.hall_pb7_edge_pending < 0xFFu)
    {
        g_sys.hall_pb7_edge_pending++;
    }
    __enable_irq();
}

uint8_t Sys_TakeHallPb7PendingEdges(void)
{
    uint8_t edges;
    __disable_irq();
    edges = g_sys.hall_pb7_edge_pending;
    g_sys.hall_pb7_edge_pending = 0u;
    __enable_irq();
    return edges;
}

void Sys_SetMacAddr(const char *addr, uint32_t len)
{
    Sys_SetString(g_sys.mac_addr, (uint32_t)sizeof(g_sys.mac_addr), addr, len);
}

uint32_t Sys_CopyMacAddr(char *dst, uint32_t dst_size)
{
    return Sys_CopyString(g_sys.mac_addr, (uint32_t)sizeof(g_sys.mac_addr), dst, dst_size);
}

void Sys_ClearMacAddr(void)
{
    Sys_SetMacAddr("", 0u);
}

void Sys_SetDeviceName(const char *name, uint32_t len)
{
    Sys_SetString(g_sys.device_name, (uint32_t)sizeof(g_sys.device_name), name, len);
}

uint32_t Sys_CopyDeviceName(char *dst, uint32_t dst_size)
{
    return Sys_CopyString(g_sys.device_name, (uint32_t)sizeof(g_sys.device_name), dst, dst_size);
}

void Sys_ClearDeviceName(void)
{
    Sys_SetDeviceName("", 0u);
}
