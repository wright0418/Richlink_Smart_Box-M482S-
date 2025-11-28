#include "movement.h"
#include "timer.h"
#include "gsensor.h"
#include "project_config.h"
#include <math.h>

static uint8_t movement_initialized = 0;
static uint32_t last_movement_time = 0;        /* last time movement was detected */
static uint32_t last_movement_sample_time = 0; /* last axis sample time */
static float movement_window[MOVEMENT_WINDOW_SAMPLES];
static uint8_t movement_window_idx = 0;
static uint8_t movement_window_count = 0;

static void ClearMovementWindow(void)
{
    movement_window_idx = 0;
    movement_window_count = 0;
    for (int i = 0; i < MOVEMENT_WINDOW_SAMPLES; i++)
        movement_window[i] = 0.0f;
}

void Movement_Init(void)
{
    movement_initialized = 1;
    last_movement_time = get_ticks_ms();
    last_movement_sample_time = 0;
    ClearMovementWindow();
}

void Movement_Reset(void)
{
    last_movement_time = get_ticks_ms();
    ClearMovementWindow();
}

void Movement_UpdateIfNeeded(void)
{
    if (!movement_initialized)
        return;

    if (!is_timeout(last_movement_sample_time, MOVEMENT_SAMPLE_INTERVAL_MS))
        return;

    last_movement_sample_time = get_ticks_ms();

    int16_t axis[3];
    GsensorReadAxis(axis);
    float mag = Gsensor_CalcMagnitude_g_from_raw(axis);

    /* push into sliding window */
    movement_window[movement_window_idx] = mag;
    movement_window_idx = (movement_window_idx + 1) % MOVEMENT_WINDOW_SAMPLES;
    if (movement_window_count < MOVEMENT_WINDOW_SAMPLES)
        movement_window_count++;

    /* compute mean and stddev */
    float sum = 0.0f;
    for (uint8_t i = 0; i < movement_window_count; i++)
        sum += movement_window[i];
    float mean = sum / (float)movement_window_count;

    float var = 0.0f;
    for (uint8_t i = 0; i < movement_window_count; i++)
    {
        float d = movement_window[i] - mean;
        var += d * d;
    }
    float stddev = 0.0f;
    if (movement_window_count > 0)
        stddev = sqrtf(var / (float)movement_window_count);

    /* Detect movement: either stddev large or magnitude deviates from 1g */
    if (stddev > MOVEMENT_STDDEV_THRESHOLD_G || fabsf(mean - 1.0f) > MOVEMENT_MAG_TOLERANCE_G)
    {
        /* movement detected -> reset last movement time */
        last_movement_time = get_ticks_ms();
    }
}

uint32_t Movement_GetLastMovementTime(void)
{
    return last_movement_time;
}

uint8_t Movement_IsInitialized(void)
{
    return movement_initialized;
}
