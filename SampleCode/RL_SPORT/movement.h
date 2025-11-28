#ifndef MOVEMENT_H
#define MOVEMENT_H

#include <stdint.h>

void Movement_Init(void);
void Movement_Reset(void);
void Movement_UpdateIfNeeded(void);
uint32_t Movement_GetLastMovementTime(void);
uint8_t Movement_IsInitialized(void);

#endif // MOVEMENT_H
