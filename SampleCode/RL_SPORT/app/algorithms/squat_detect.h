#ifndef SQUAT_DETECT_H
#define SQUAT_DETECT_H

#include <stdint.h>

typedef enum
{
    SQUAT_PHASE_IDLE = 0,
    SQUAT_PHASE_STAND,
    SQUAT_PHASE_DESCEND,
    SQUAT_PHASE_BOTTOM,
    SQUAT_PHASE_ASCEND
} SquatPhase;

typedef struct
{
    float raw_mag_g;
    float grav_mag_g;
    float vert_acc_g;
    float depth_score;
    float rms_g;
} SquatFeatureSnapshot;

void SquatDetect_Init(void);
void SquatDetect_Reset(void);
uint8_t SquatDetect_ProcessSample(int16_t ax, int16_t ay, int16_t az, uint32_t now_ms);
uint16_t SquatDetect_GetCount(void);
SquatPhase SquatDetect_GetPhase(void);
uint8_t SquatDetect_GetProgress8(void);
void SquatDetect_GetFeatureSnapshot(SquatFeatureSnapshot *out);

#endif /* SQUAT_DETECT_H */
