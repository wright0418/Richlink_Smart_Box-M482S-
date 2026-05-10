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

    uint32_t sample_interval_ms;
    uint32_t last_sample_ms;
    uint32_t display_interval_ms;
    uint32_t last_display_ms;

    uint8_t flash_active;
    uint32_t flash_start_ms;
    uint32_t flash_hold_ms;
    WS2812B_Color flash_color;

    uint8_t last_render_valid;
    uint8_t last_render_flash;
    uint16_t last_render_count;
    SquatPhase last_render_phase;
    uint8_t last_render_progress;

    int16_t last_axis[3];
    float last_mag_g;

    uint8_t last_state_valid;
    SquatPhase last_state_phase;
    uint16_t last_state_count;
    uint8_t last_state_prog;
} SquatModeCtx;

static SquatModeCtx s_mode;

static uint32_t ms_from_hz(uint32_t hz)
{
    uint32_t interval;
    if (hz == 0u)
    {
        return 20u;
    }

    interval = (1000u + (hz / 2u)) / hz;
    return (interval == 0u) ? 1u : interval;
}

static void schedule_flash(WS2812B_Color color, uint32_t hold_ms, uint32_t now_ms)
{
    if (hold_ms == 0u)
    {
        s_mode.flash_active = 0u;
        return;
    }

    s_mode.flash_color = color;
    s_mode.flash_hold_ms = hold_ms;
    s_mode.flash_start_ms = now_ms;
    s_mode.flash_active = 1u;
    s_mode.last_render_valid = 0u;
}

static void render_squat_display(uint32_t now_ms, uint16_t count, SquatPhase phase, uint8_t progress)
{
    uint8_t should_refresh = 0u;

    if (s_mode.flash_active && is_timeout(s_mode.flash_start_ms, s_mode.flash_hold_ms))
    {
        s_mode.flash_active = 0u;
        s_mode.last_render_valid = 0u;
    }

    if (!s_mode.last_render_valid)
    {
        should_refresh = 1u;
    }
    else if (!is_timeout(s_mode.last_display_ms, s_mode.display_interval_ms))
    {
        return;
    }
    else if (s_mode.flash_active)
    {
        should_refresh = (s_mode.last_render_flash == 0u) ? 1u : 0u;
    }
    else
    {
        should_refresh = (s_mode.last_render_flash != 0u) ||
                         (count != s_mode.last_render_count) ||
                         (phase != s_mode.last_render_phase) ||
                         (progress != s_mode.last_render_progress);
    }

    if (!should_refresh)
    {
        return;
    }

    if (s_mode.flash_active)
    {
        WS2812B_FillColor(s_mode.flash_color);
        (void)WS2812B_Refresh();
        s_mode.last_render_flash = 1u;
    }
    else
    {
        WS2812B_ShowSquatScreen(count, phase, progress);
        s_mode.last_render_flash = 0u;
    }

    s_mode.last_display_ms = now_ms;
    s_mode.last_render_valid = 1u;
    s_mode.last_render_count = count;
    s_mode.last_render_phase = phase;
    s_mode.last_render_progress = progress;
}

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
    uint32_t now_ms = get_ticks_ms();

    memset(&s_mode, 0, sizeof(s_mode));
    s_mode.state = SQUAT_MODE_IDLE;
    s_mode.stream_interval_ms = 200u;
    s_mode.remote_phase = SQUAT_PHASE_IDLE;
    s_mode.sample_interval_ms = ms_from_hz(SQUAT_SAMPLE_RATE_HZ);
    s_mode.display_interval_ms = SQUAT_DISPLAY_FRAME_INTERVAL_MS;
    s_mode.last_sample_ms = now_ms;
    s_mode.last_display_ms = now_ms;

    WS2812B_Init();
    WS2812B_Clear();
    (void)WS2812B_Refresh();

    SquatDetect_Init();
}

void SquatMode_Start(void)
{
    uint32_t now_ms = get_ticks_ms();

    SquatDetect_Init();
    SquatDetect_Reset();
    s_mode.state = SQUAT_MODE_RUNNING;
    s_mode.remote_display_enabled = 0u;
    s_mode.last_sample_ms = now_ms;
    s_mode.last_render_valid = 0u;
    schedule_flash(WS2812B_ColorMake(0u, 200u, 0u), 120u, now_ms);
}

void SquatMode_Stop(void)
{
    s_mode.state = SQUAT_MODE_IDLE;
    s_mode.remote_display_enabled = 0u;
    s_mode.flash_active = 0u;
    s_mode.last_render_valid = 0u;
    WS2812B_Clear();
    (void)WS2812B_Refresh();
}

void SquatMode_Reset(void)
{
    uint32_t now_ms = get_ticks_ms();

    SquatDetect_Init();
    SquatDetect_Reset();
    s_mode.remote_display_enabled = 0u;
    s_mode.last_sample_ms = now_ms;
    s_mode.last_render_valid = 0u;
    schedule_flash(WS2812B_ColorMake(255u, 32u, 0u), 120u, now_ms);
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
    uint8_t sample_ok = 0u;
    uint16_t display_count;
    SquatPhase display_phase;
    uint8_t display_progress;

    if (s_mode.state == SQUAT_MODE_IDLE)
    {
        stream_once(now_ms);
        return;
    }

    if (is_timeout(s_mode.last_sample_ms, s_mode.sample_interval_ms))
    {
        s_mode.last_sample_ms = now_ms;
        sample_ok = GsensorReadAxisChecked(axis);
        if (sample_ok)
        {
            s_mode.last_axis[0] = axis[0];
            s_mode.last_axis[1] = axis[1];
            s_mode.last_axis[2] = axis[2];
            s_mode.last_mag_g = Gsensor_CalcMagnitude_g_from_raw(axis);
            rep = SquatDetect_ProcessSample(axis[0], axis[1], axis[2], now_ms);
        }
    }

    if (rep)
    {
        schedule_flash(WS2812B_ColorMake(0u, 255u, 0u), 80u, now_ms);
    }

    if (s_mode.remote_display_enabled)
    {
        display_count = s_mode.remote_count;
        display_phase = s_mode.remote_phase;
        display_progress = s_mode.remote_progress;
    }
    else
    {
        display_count = SquatDetect_GetCount();
        display_phase = SquatDetect_GetPhase();
        display_progress = SquatDetect_GetProgress8();
    }

    render_squat_display(now_ms, display_count, display_phase, display_progress);

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
