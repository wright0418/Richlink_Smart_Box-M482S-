/**
 * @file ble.h
 * @brief Public BLE helper API and command constants
 *
 * This header contains helper functions used to interact with the UART-
 * connected BLE module. It focuses on sending AT commands and processing
 * received responses; lower-level UART handling is implemented elsewhere.
 */

#ifndef BLE_H
#define BLE_H

#include <stdint.h>

/* BLE Command Strings */
#define BLE_CMD_NAME_QUERY "AT+NAME=?\r\n"
#define BLE_CMD_ADDR_QUERY "AT+ADDR=?\r\n"
#define BLE_CMD_MODE_DATA "AT+MODE_DATA\r\n"
#define BLE_CMD_REBOOT "AT+REBOOT\r\n"
#define BLE_CMD_DISC "AT+DISC\r\n"
#define BLE_CMD_ADVERT_ON "AT+ADVERT=1\r\n"
#define BLE_CMD_ADVERT_OFF "AT+ADVERT=0\r\n"
#define BLE_CMD_DLPS_ON "AT+DLPS_EN=1\r\n"
#define BLE_CMD_DLPS_OFF "AT+DLPS_EN=0\r\n"
#define BLE_CMD_CCMD "!CCMD@"

#define RXBUFSIZE 512
#define BUF_SIZE 512 /* Size of DBG_PRINT buffer */

/* UART receive buffer (defined in ble.c) - make available to other modules */
extern volatile uint8_t g_u8RecData[RXBUFSIZE];
extern volatile uint32_t g_u32RecLen;
extern volatile uint8_t g_u8DataReady;

/* Public BLE API used by main and other modules */
/**
 * @brief Initialize BLE helper state and ensure module is in run mode.
 *
 * Safe to call from main startup. May enable UART interrupts and
 * initialize internal buffers.
 */
void BleSetup(void);

/**
 * @brief Process pending BLE UART input and update state.
 *
 * Call from the main loop; this function parses complete lines and
 * updates `SystemStatus` accordingly.
 */
void CheckBleRecvMsg(void);

/**
 * @brief Formatted send helper for BLE UART.
 * @param uart Opaque UART handle (SDK type). May be cast inside impl.
 * @param format printf-style format string.
 * @return Number of bytes written or negative on error.
 */
int BLE_UART_SEND(void *uart, const char *format, ...);

/**
 * @brief Initialize BLE transport (UART1) and interrupts
 * @param baud Baud rate for UART1
 */
void Ble_Init(uint32_t baud);

/**
 * @brief Send a NUL-terminated string to the BLE module.
 * @param data NUL-terminated C string to send.
 */
void BLESendData(const char *data);

/**
 * @brief Set the BLE device name via AT command sequence.
 * @param name NUL-terminated string for new device name.
 */
void BLESetName(const char *name);

/* Mode/connection helpers */
void BLEDisconnect(void);
void BLEToRunMode(void);
void BLE_to_DLPS(void);
void BLE_DISCONNECT(void);

#endif /* BLE_H */
