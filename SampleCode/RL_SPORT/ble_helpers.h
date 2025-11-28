#ifndef BLE_HELPERS_H
#define BLE_HELPERS_H

#include <stdint.h>

#define BLE_SEND_INTERVAL_MS 100

void BLE_SendSensorData(int16_t axis[3]);
void BLE_SendJumpCount(void);

#endif // BLE_HELPERS_H
