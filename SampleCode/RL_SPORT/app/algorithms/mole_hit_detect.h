#ifndef MOLE_HIT_DETECT_H
#define MOLE_HIT_DETECT_H

#include <stdint.h>

typedef enum
{
    MOLE_HIT_SOURCE_BUTTON = 1u,
    MOLE_HIT_SOURCE_GSENSOR = 2u
} MoleHitSource;

typedef struct
{
    uint8_t has_last_mag;
    float last_mag_g;
    uint32_t last_hit_ms;
    MoleHitSource last_source;
} MoleHitDetector;

void MoleHitDetector_Init(MoleHitDetector *detector);
uint8_t MoleHitDetector_ProcessButton(MoleHitDetector *detector, uint32_t now_ms);
uint8_t MoleHitDetector_ProcessAccelMag(MoleHitDetector *detector, uint32_t now_ms, float mag_g);

#endif /* MOLE_HIT_DETECT_H */
