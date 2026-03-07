/**
 * @file ble.h
 * @brief Public BLE helper API and command constants
 *
 * This header contains helper functions used to interact with the UART-
 * connected BLE module. It focuses on sending AT commands and processing
 * received responses; lower-level UART handling and UART driver code are
 * implemented elsewhere (platform-specific).
 *
 * The module provides lightweight helpers to initialize the transport,
 * send formatted commands, and parse incoming responses into higher-level
 * events used by the application. Public globals represent the receive
 * buffer state and are intended for read-only observation by other
 * modules (writes should be done through the module API).
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

/**
 * @name BLE UART receive buffer (shared state)
 * These symbols are defined in ble.c; other modules may observe them but
 * should prefer the API functions (CheckBleRecvMsg, BLESendData, etc.)
 */
/*@{*/
extern volatile uint8_t g_u8RecData[RXBUFSIZE]; /**< RX buffer (raw bytes) */
extern volatile uint32_t g_u32RecLen;           /**< Number of bytes currently in RX buffer */
extern volatile uint8_t g_u8DataReady;          /**< Non-zero when a complete message is available */
/*@}*/

/* Public BLE API used by main and other modules */
/**
 * @brief Initialize BLE helper state and ensure module is in run mode.
 *
 * Safe to call from main startup. May enable UART interrupts and
 * initialize internal buffers.
 */
/**
 * @brief Initialize BLE helper state and ensure module is in run mode.
 *
 * Safe to call from main startup. This configures internal buffers and
 * may enable UART interrupts used by the BLE transport. Implementation
 * details (which UART instance, IRQ priorities) are platform-dependent.
 */
void BleSetup(void);

/**
 * @brief Process pending BLE UART input and update state.
 *
 * Call from the main loop; this function parses complete lines and
 * updates `SystemStatus` accordingly.
 */
/**
 * @brief Process pending BLE UART input and update state.
 *
 * Called from the main loop to parse buffered UART input. This function
 * identifies complete lines or messages, updates internal state and
 * may call system-level handlers (e.g. Sys_SetBleState) based on parsed
 * responses.
 */
void CheckBleRecvMsg(void);

/**
 * @brief Formatted send helper for BLE UART.
 * @param uart Opaque UART handle (SDK type). May be cast inside impl.
 * @param format printf-style format string.
 * @return Number of bytes written or negative on error.
 */
/**
 * @brief Formatted send helper for BLE UART.
 * @param uart Opaque UART handle (SDK-specific type). The implementation
 *             may cast this to the platform UART type.
 * @param format printf-style format string followed by arguments.
 * @return Number of bytes written on success, negative on error.
 */
int BLE_UART_SEND(void *uart, const char *format, ...);

/**
 * @brief Initialize BLE transport (UART1) and interrupts
 * @param baud Baud rate for UART1
 */
/**
 * @brief Initialize BLE transport (UART) and related interrupts.
 * @param baud Baud rate for UART used to communicate with the BLE module.
 *
 * This function configures the UART peripheral used for the BLE link
 * (commonly UART1 on the board) and enables any required IRQs.
 */
void Ble_Init(uint32_t baud);

/**
 * @brief Send a NUL-terminated string to the BLE module.
 * @param data NUL-terminated C string to send.
 */
/**
 * @brief Send a NUL-terminated string to the BLE module.
 * @param data NUL-terminated C string to send. The function may block
 *             briefly while queuing the data to the UART.
 */
void BLESendData(const char *data);

/**
 * @brief Set the BLE device name via AT command sequence.
 * @param name NUL-terminated string for new device name.
 */
/**
 * @brief Set the BLE device name via AT command sequence.
 * @param name NUL-terminated string for the new device name. The
 *             implementation will format and send the appropriate AT
 *             command and may trigger a module reboot if required.
 */
void BLESetName(const char *name);

/**
 * @brief Run BLE rename flow: query name/MAC and rename to ROPE_xxxx if needed.
 * @param device_name Output buffer (at least 5 bytes) for device name suffix.
 * @param mac Output buffer (at least 5 bytes) for MAC suffix.
 */
void Ble_RenameFlow(uint8_t *device_name, uint8_t *mac);

/**
 * @brief Start non-blocking BLE rename flow.
 *
 * The flow runs as a small state machine and must be progressed by
 * periodically calling Ble_RenameFlowProcess() from main loop.
 */
void Ble_RenameFlowStart(void);

/**
 * @brief Progress non-blocking BLE rename flow state machine.
 *
 * Call this periodically from main loop after BLE transport is initialized.
 */
void Ble_RenameFlowProcess(void);

/**
 * @brief Query whether non-blocking BLE rename flow has completed.
 * @return 1 if completed (success or skipped), 0 otherwise.
 */
uint8_t Ble_RenameFlowIsDone(void);

/* Mode/connection helpers */
/** Switch BLE link to disconnected state (soft disconnect). */
void BLEDisconnect(void);
/** Ensure BLE module is in run (normal) mode. */
void BLEToRunMode(void);
/** Request BLE module to enter DLPS (deep low-power sleep). */
void BLE_to_DLPS(void);
/** Alias for BLEDisconnect (historical compatibility). */
void BLE_DISCONNECT(void);

#endif /* BLE_H */
