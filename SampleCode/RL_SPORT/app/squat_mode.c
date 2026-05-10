#include "squat_mode.h"

#include "../project_config.h"
#include "../drivers/gsensor.h"
#include "../drivers/ws2812b.h"
#include "../drivers/ws2812b_digits.h"
#include "../drivers/timer.h"
#include "../ble.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    SquatModeState state;
    SquatStreamType stream_type;
    uint32_t stream_interval_ms;
    uint32_t last_stream_ms;

    uint8_t remote_display_enabled;
    uint16_t remote_count;
    SquatPhase remote_phase;
    uint8_t remote_progress;

    int16_t last_axis[3];
    float last_mag_g;

    uint8_t last_state_valid;
    SquatPhase last_state_phase;
    uint16_t last_state_count;
    uint8_t last_state_prog;
} SquatModeCtx;

static SquatModeCtx s_mode;

static const char *phase_name(SquatPhase p)
{
    switch (p)
    {
    case SQUAT_PHASE_STAND:
        return "STAND";
    case SQUAT_PHASE_DESCEND:
        return "DESCEND";
    case SQUAT_PHASE_BOTTOM:
        return "BOTTOM";
    case SQUAT_PHASE_ASCEND:
        return "ASCEND";
    case SQUAT_PHASE_IDLE:
    default:
        return "IDLE";
    }
}

void SquatMode_Init(void)
{
    memset(&s_mode, 0, sizeof(s_mode));
    s_mode.state = SQUAT_MODE_IDLE;
    s_mode.stream_interval_ms = 200u;
    s_mode.remote_phase = SQUAT_PHASE_IDLE;

    WS2812B_Init();
    WS2812B_Clear();
    (void)WS2812B_Refresh();

    SquatDetect_Init();
}

void SquatMode_Start(void)
{
    SquatDetect_Init();
    SquatDetect_Reset();
    s_mode.state = SQUAT_MODE_RUNNING;
    s_mode.remote_display_enabled = 0u;
    WS2812B_ShowSquatFlash(WS2812B_ColorMake(0u, 200u, 0u), 120u);
}

void SquatMode_Stop(void)
{
    s_mode.state = SQUAT_MODE_IDLE;
    s_mode.remote_display_enabled = 0u;
    WS2812B_Clear();
    (void)WS2812B_Refresh();
}

void SquatMode_Reset(void)
{
    SquatDetect_Init();
    SquatDetect_Reset();
    s_mode.remote_display_enabled = 0u;
    WS2812B_ShowSquatFlash(WS2812B_ColorMake(255u, 32u, 0u), 120u);
}

void SquatMode_OnKeyEvent(uint32_t now_ms)
{
    (void)now_ms;
    if (s_mode.state == SQUAT_MODE_IDLE)
    {
        SquatMode_Start();
    }
    else
    {
        SquatMode_Reset();
    }
}

static void stream_once(uint32_t now_ms)
{
    char line[160];
    SquatFeatureSnapshot feat;
    if (s_mode.stream_type == SQUAT_STREAM_NONE)
    {
        return;
    }
    if (!is_timeout(s_mode.last_stream_ms, s_mode.stream_interval_ms))
    {
        return;
    }
    s_mode.last_stream_ms = now_ms;

    switch (s_mode.stream_type)
    {
    case SQUAT_STREAM_RAW:
        snprintf(line, sizeof(line), "+DATA,SQUAT_RAW,AX=%d,AY=%d,AZ=%d,MAG=%.3f\r\n",
                 s_mode.last_axis[0], s_mode.last_axis[1], s_mode.last_axis[2], (double)s_mode.last_mag_g);
        BLESendData(line);
        break;

    case SQUAT_STREAM_FEATURE:
        SquatMode_GetFeatureSnapshot(&feat);
        snprintf(line, sizeof(line),
                 "+DATA,SQUAT_FEAT,GMAG=%.3f,VACC=%.3f,RMS=%.3f,DEPTH=%.3f\r\n",
                 (double)feat.grav_mag_g, (double)feat.vert_acc_g, (double)feat.rms_g, (double)feat.depth_score);
        BLESendData(line);
        break;

    case SQUAT_STREAM_STATE:
    {
        SquatPhase phase = SquatDetect_GetPhase();
        uint16_t count = SquatDetect_GetCount();
        uint8_t prog = SquatDetect_GetProgress8();

        if ((!s_mode.last_state_valid) ||
            (phase != s_mode.last_state_phase) ||
            (count != s_mode.last_state_count) ||
            (prog != s_mode.last_state_prog))
        {
            snprintf(line, sizeof(line), "+DATA,SQUAT_STATE,STATE=%s,COUNT=%u,PROG=%u\r\n",
                     phase_name(phase),
                     (unsigned)count,
                     (unsigned)prog);
            BLESendData(line);

            s_mode.last_state_valid = 1u;
            s_mode.last_state_phase = phase;
            s_mode.last_state_count = count;
            s_mode.last_state_prog = prog;
        }
    }
    break;

    default:
        break;
    }
}

void SquatMode_Process(uint32_t now_ms)
{
    int16_t axis[3] = {0};
    uint8_t rep = 0u;

    if (s_mode.state == SQUAT_MODE_IDLE)
    {
        stream_once(now_ms);
        return;
    }

    GsensorReadAxis(axis);
    s_mode.last_axis[0] = axis[0];
    s_mode.last_axis[1] = axis[1];
    s_mode.last_axis[2] = axis[2];
    s_mode.last_mag_g = Gsensor_CalcMagnitude_g_from_raw(axis);

    rep = SquatDetect_ProcessSample(axis[0], axis[1], axis[2], now_ms);

    if (s_mode.remote_display_enabled)
    {
        WS2812B_ShowSquatScreen(s_mode.remote_count, s_mode.remote_phase, s_mode.remote_progress);
        stream_once(now_ms);
        return;
    }

    WS2812B_ShowSquatScreen(SquatDetect_GetCount(), SquatDetect_GetPhase(), SquatDetect_GetProgress8());

    if (rep)
    {
        WS2812B_ShowSquatFlash(WS2812B_ColorMake(0u, 255u, 0u), 80u);
        WS2812B_ShowSquatScreen(SquatDetect_GetCount(), SquatDetect_GetPhase(), SquatDetect_GetProgress8());
    }

    stream_once(now_ms);
}

uint16_t SquatMode_GetCount(void)
{
    return SquatDetect_GetCount();
}

SquatPhase SquatMode_GetPhase(void)
{
    return SquatDetect_GetPhase();
}

uint8_t SquatMode_GetProgress8(void)
{
    return SquatDetect_GetProgress8();
}

SquatModeState SquatMode_GetState(void)
{
    return s_mode.state;
}

void SquatMode_SetStream(SquatStreamType type, uint32_t interval_ms)
{
    if (interval_ms < 50u)
    {
        interval_ms = 50u;
    }
    if (interval_ms > 2000u)
    {
        interval_ms = 2000u;
    }
    s_mode.stream_type = type;
    s_mode.stream_interval_ms = interval_ms;
    s_mode.last_stream_ms = get_ticks_ms();
    s_mode.last_state_valid = 0u;
}

void SquatMode_StopStream(void)
{
    s_mode.stream_type = SQUAT_STREAM_NONE;
}

SquatStreamType SquatMode_GetStreamType(void)
{
    return s_mode.stream_type;
}

uint32_t SquatMode_GetStreamIntervalMs(void)
{
    return s_mode.stream_interval_ms;
}

void SquatMode_GetLastRaw(int16_t *axis3, float *mag_g)
{
    if (axis3 != NULL)
    {
        axis3[0] = s_mode.last_axis[0];
        axis3[1] = s_mode.last_axis[1];
        axis3[2] = s_mode.last_axis[2];
    }
    if (mag_g != NULL)
    {
        *mag_g = s_mode.last_mag_g;
    }
}

void SquatMode_GetFeatureSnapshot(SquatFeatureSnapshot *out)
{
    SquatDetect_GetFeatureSnapshot(out);
}

void SquatMode_SetRemoteDisplay(uint16_t count, SquatPhase phase, uint8_t progress)
{
    if (count > 99u)
    {
        count = 99u;
    }
    if (progress > 8u)
    {
        progress = 8u;
    }

    s_mode.remote_count = count;
    s_mode.remote_phase = phase;
    s_mode.remote_progress = progress;
    s_mode.remote_display_enabled = 1u;
}

void SquatMode_ClearRemoteDisplay(void)
{
    s_mode.remote_display_enabled = 0u;
}

uint8_t SquatMode_IsRemoteDisplayEnabled(void)
{
    return s_mode.remote_display_enabled;
}
