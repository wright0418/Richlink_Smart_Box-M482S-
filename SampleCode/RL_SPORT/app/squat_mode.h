#ifndef SQUAT_MODE_H
#define SQUAT_MODE_H

#include <stdint.h>
#include "algorithms/squat_detect.h"

typedef enum
{
    SQUAT_MODE_IDLE = 0,
    SQUAT_MODE_RUNNING
} SquatModeState;

typedef enum
{
    SQUAT_STREAM_NONE = 0,
    SQUAT_STREAM_RAW,
    SQUAT_STREAM_FEATURE,
    SQUAT_STREAM_STATE
} SquatStreamType;

void SquatMode_Init(void);
void SquatMode_Process(uint32_t now_ms);
void SquatMode_OnKeyEvent(uint32_t now_ms);
void SquatMode_Start(void);
void SquatMode_Stop(void);
void SquatMode_Reset(void);

uint16_t SquatMode_GetCount(void);
SquatPhase SquatMode_GetPhase(void);
uint8_t SquatMode_GetProgress8(void);
SquatModeState SquatMode_GetState(void);

void SquatMode_SetStream(SquatStreamType type, uint32_t interval_ms);
void SquatMode_StopStream(void);
SquatStreamType SquatMode_GetStreamType(void);
uint32_t SquatMode_GetStreamIntervalMs(void);

void SquatMode_GetLastRaw(int16_t *axis3, float *mag_g);
void SquatMode_GetFeatureSnapshot(SquatFeatureSnapshot *out);

void SquatMode_SetRemoteDisplay(uint16_t count, SquatPhase phase, uint8_t progress);
void SquatMode_ClearRemoteDisplay(void);
uint8_t SquatMode_IsRemoteDisplayEnabled(void);

#endif /* SQUAT_MODE_H */
