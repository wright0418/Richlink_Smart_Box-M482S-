#include "ble_helpers.h"
#include "ble.h"
#include "system_status.h"
#include <stdio.h>

void BLE_SendSensorData(int16_t axis[3])
{
    char bleData[64];
    snprintf(bleData, sizeof(bleData), "send,%d,%d,%d,%d\n",
             Sys_GetJumpTimes() >> 1, axis[0], axis[1], axis[2]);
    BLESendData(bleData);
}

void BLE_SendJumpCount(void)
{
    char bleData[32];
    snprintf(bleData, sizeof(bleData), "send,%d\n", Sys_GetJumpTimes());
    BLESendData(bleData);
}
