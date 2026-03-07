#ifndef _BLE_AT_REPL_H_
#define _BLE_AT_REPL_H_

#include <stdint.h>

void BleAtRepl_Init(void);
uint8_t BleAtRepl_IsActive(void);
uint8_t BleAtRepl_HandleMessage(const char *msg);
void BleAtRepl_RunIfActive(void);

#endif /* _BLE_AT_REPL_H_ */
