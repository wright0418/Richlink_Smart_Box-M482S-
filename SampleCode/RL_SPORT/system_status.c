/**
 * @file system_status.c
 * @brief System status module implementation
 */
#include "system_status.h"
#include "NuMicro.h"
#include <string.h>

/* Global system status instance (extern declaration in header) */
SystemStatus g_sys;

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

void Sys_Init(void)
{
    /* Initialize all system status fields to default values */
    g_sys.ble_state = BLE_DISCONNECTED;
    g_sys.ble_mode = 0;
    g_sys.game_state = GAME_STOP;
    g_sys.keyA_state = 0;
    g_sys.jump_times = 0;
    g_sys.left_time_ms = 0;
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

void Sys_SetDeviceName(const char *name, uint32_t len)
{
    Sys_SetString(g_sys.device_name, (uint32_t)sizeof(g_sys.device_name), name, len);
}

uint32_t Sys_CopyDeviceName(char *dst, uint32_t dst_size)
{
    return Sys_CopyString(g_sys.device_name, (uint32_t)sizeof(g_sys.device_name), dst, dst_size);
}
