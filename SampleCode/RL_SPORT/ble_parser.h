/**
 * @file ble_parser.h
 * @brief Pure BLE text parsing helpers for response classification and suffix extraction.
 *
 * This module intentionally contains only string parsing logic so it can be
 * unit-tested on host without MCU/UART dependencies.
 */
#ifndef BLE_PARSER_H
#define BLE_PARSER_H

#include <stdint.h>

typedef enum
{
    BLE_CMD_NONE = 0,
    BLE_CMD_CONNECTED,
    BLE_CMD_DISCONNECTED,
    BLE_CMD_CMD_MODE,
    BLE_CMD_DATA_MODE,
    BLE_CMD_CONN_START,
    BLE_CMD_GET_CYCLE,
    BLE_CMD_SET_END,
    BLE_CMD_DISC_MSG,
    BLE_CMD_MAC_ADDR,
    BLE_CMD_DEVICE_NAME
} BleCmdType;

BleCmdType BleParser_ParseCommand(const char *msg);
void BleParser_StripCmdModeMarker(char *msg, const char *marker);
uint8_t BleParser_ExtractMacSuffix4(const char *src, char *out4);
uint8_t BleParser_ExtractRopeSuffix4(const char *name, char *out4);
uint8_t BleParser_IsNameQueryEcho(const char *s);
uint8_t BleParser_IsAddrQueryEcho(const char *s);

#endif /* BLE_PARSER_H */