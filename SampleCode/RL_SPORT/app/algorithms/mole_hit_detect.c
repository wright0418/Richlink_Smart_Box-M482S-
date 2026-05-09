#include "mole_hit_detect.h"

#include "../../project_config.h"

#include <math.h>
#include <stddef.h>

static uint8_t MoleHitDetector_AcceptHit(MoleHitDetector *detector, uint32_t now_ms, MoleHitSource source)
{
    uint32_t elapsed;

    if (detector == NULL)
    {
        return 0u;
    }

    elapsed = now_ms - detector->last_hit_ms;
    if ((detector->last_hit_ms != 0u) && (elapsed < MOLE_HIT_DEBOUNCE_MS))
    {
        return 0u;
    }

    if ((detector->last_hit_ms != 0u) &&
        (detector->last_source != source) &&
        (elapsed < MOLE_HIT_MERGE_WINDOW_MS))
    {
        return 0u;
    }

    detector->last_hit_ms = now_ms;
    detector->last_source = source;
    return 1u;
}

void MoleHitDetector_Init(MoleHitDetector *detector)
{
    if (detector == NULL)
    {
        return;
    }

    detector->has_last_mag = 0u;
    detector->last_mag_g = 0.0f;
    detector->last_hit_ms = 0u;
    detector->last_source = MOLE_HIT_SOURCE_BUTTON;
}

uint8_t MoleHitDetector_ProcessButton(MoleHitDetector *detector, uint32_t now_ms)
{
#if MOLE_HIT_BUTTON_ENABLE
    return MoleHitDetector_AcceptHit(detector, now_ms, MOLE_HIT_SOURCE_BUTTON);
#else
    (void)detector;
    (void)now_ms;
    return 0u;
#endif
}

uint8_t MoleHitDetector_ProcessAccelMag(MoleHitDetector *detector, uint32_t now_ms, float mag_g)
{
#if MOLE_HIT_GSENSOR_ENABLE
    float jerk_delta;
    float gravity_delta;
    uint8_t hit;

    if (detector == NULL)
    {
        return 0u;
    }

    if (detector->has_last_mag == 0u)
    {
        detector->last_mag_g = mag_g;
        detector->has_last_mag = 1u;
        return 0u;
    }

    jerk_delta = fabsf(mag_g - detector->last_mag_g);
    gravity_delta = fabsf(mag_g - 1.0f);
    detector->last_mag_g = mag_g;

    hit = ((jerk_delta >= MOLE_HIT_JERK_THRESHOLD_G) ||
           (gravity_delta >= MOLE_HIT_MAG_DELTA_THRESHOLD_G))
              ? 1u
              : 0u;

    if (hit == 0u)
    {
        return 0u;
    }

    return MoleHitDetector_AcceptHit(detector, now_ms, MOLE_HIT_SOURCE_GSENSOR);
#else
    (void)detector;
    (void)now_ms;
    (void)mag_g;
    return 0u;
#endif
}
