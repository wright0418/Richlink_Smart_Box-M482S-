/**
 * @file test_mode.h
 * @brief UART0-driven hardware test mode (menu).
 */
#ifndef _TEST_MODE_H_
#define _TEST_MODE_H_

#include <stdint.h>

/**
 * @brief Poll UART0 for the "test" command to enter test mode.
 */
void TestMode_PollEnter(void);

/**
 * @brief Run the UART0 test menu loop if active.
 */
void TestMode_RunMenuIfActive(void);

#endif // _TEST_MODE_H_
