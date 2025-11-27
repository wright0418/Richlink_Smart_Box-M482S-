/**
 * @file system_status.c
 * @brief System status module implementation
 */
#include "system_status.h"
#include <string.h>

/* Global system status instance (extern declaration in header) */
SystemStatus g_sys;

void Sys_Init(void)
{
    /* Initialize all system status fields to default values */
    g_sys.ble_state = BLE_DISCONNECTED;
    g_sys.ble_mode = 0;
    g_sys.game_state = GAME_STOP;
    g_sys.keyA_state = 0;
    g_sys.jump_times = 0;
    g_sys.lest_time_ms = 0;
    memset((void *)g_sys.mac_addr, 0, sizeof(g_sys.mac_addr));
    memset((void *)g_sys.device_name, 0, sizeof(g_sys.device_name));
    g_sys.jump_flag = 0;
    g_sys.keyA_flag = 0;
    g_sys.gsensor_flag = 0;
}

const char *Sys_GetMacAddr(void)
{
    return (const char *)g_sys.mac_addr;
}

void Sys_SetMacAddr(const char *addr, uint32_t len)
{
    if (addr && len > 0 && len < sizeof(g_sys.mac_addr))
    {
        memcpy((void *)g_sys.mac_addr, addr, len);
        g_sys.mac_addr[len] = '\0';
    }
}

const char *Sys_GetDeviceName(void)
{
    return (const char *)g_sys.device_name;
}

void Sys_SetDeviceName(const char *name, uint32_t len)
{
    if (name && len > 0 && len < sizeof(g_sys.device_name))
    {
        memcpy((void *)g_sys.device_name, name, len);
        g_sys.device_name[len] = '\0';
    }
}
