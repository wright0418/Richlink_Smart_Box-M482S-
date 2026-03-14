/**
 * @file test_mode.h
 * @brief UART0-driven hardware test mode (interactive menu + AT auto-test).
 *
 * Two modes of operation:
 *  1. Interactive menu  — type "test" on UART0 to enter number-based menu.
 *  2. AT auto-test      — send "AT+TEST=<CMD>\r\n" for structured responses.
 *
 * See docs/UART_AUTO_TEST_PROTOCOL.md for the full AT protocol specification.
 */
#ifndef _TEST_MODE_H_
#define _TEST_MODE_H_

#include <stdint.h>
#include "project_config.h"

/* FW_VERSION / BOARD_NAME are defined in project_config.h */
#define TEST_FW_VERSION FW_VERSION
#define TEST_BOARD_NAME BOARD_NAME

/**
 * @brief Poll UART0 for the "test" command OR "AT+TEST=" prefix.
 *
 * Call from main loop. If an AT+TEST= line is detected it is dispatched
 * immediately; if "test" is detected the interactive menu flag is set.
 */
void TestMode_PollEnter(void);

/**
 * @brief Run the UART0 interactive test menu loop if active.
 */
void TestMode_RunMenuIfActive(void);

#endif // _TEST_MODE_H_
