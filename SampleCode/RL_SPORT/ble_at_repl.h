/**
 * @file ble_at_repl.h
 * @brief BLE AT command REPL support interface.
 *
 * When enabled, the BLE module can receive AT+TEST commands and reply with
 * structured +OK/+ERR responses. This header exposes the public REPL state
 * and processing hooks used by the main loop.
 */
#ifndef _BLE_AT_REPL_H_
#define _BLE_AT_REPL_H_

#include <stdint.h>

/**
 * @brief Initialize BLE REPL module state.
 */
void BleAtRepl_Init(void);

/**
 * @brief Query whether BLE REPL mode is currently active.
 * @return 1 if REPL mode is active, 0 otherwise.
 */
uint8_t BleAtRepl_IsActive(void);

/**
 * @brief Handle a received BLE AT+TEST line.
 * @param msg Input line including the AT+TEST prefix.
 * @return 1 if the message was consumed by the REPL module, 0 otherwise.
 */
uint8_t BleAtRepl_HandleMessage(const char *msg);

/**
 * @brief Perform periodic REPL background work when active.
 *
 * This may stream sensor data, enforce BLE-connected state requirements, and
 * keep the REPL state machine alive.
 */
void BleAtRepl_RunIfActive(void);

#endif /* _BLE_AT_REPL_H_ */
