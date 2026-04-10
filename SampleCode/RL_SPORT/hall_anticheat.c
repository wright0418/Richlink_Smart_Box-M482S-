/**
 * @file hall_anticheat.c
 * @brief Hall sensor anti-cheat validation implementation
 *
 * Uses CMSIS DSP FIR low-pass filter + peak detection on G-sensor data to
 * independently estimate jump count, then caps the Hall count to prevent
 * cheating by manual magnet rotation.
 */

#include "hall_anticheat.h"

#if USE_HALL_ANTICHEAT

#include <math.h>
#include <string.h>
#include "gsensor.h"
#include "timer.h"

extern float sqrtf(float);

#ifdef ARM_MATH_CM4
#include "../../../Library/CMSIS/Include/arm_math.h"
#endif

/* Fallback types/stubs when CMSIS DSP headers are not visible to linter */
#ifndef ARM_MATH_CM4
typedef float float32_t;
static inline void arm_sqrt_f32(float32_t in, float32_t *out) { *out = sqrtf(in); }
typedef struct
{
    uint16_t numTaps;
    float32_t *pState;
    float32_t *pCoeffs;
} arm_fir_instance_f32;
#endif

/*---------------------------------------------------------------------------*/
/* Sensor sensitivity — use shared gsensor.h conversion for consistency     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* FIR low-pass filter (7-tap Hamming, 6 Hz cutoff at 50 Hz Fs)             */
/*---------------------------------------------------------------------------*/
static const float32_t s_fir_coeffs[ANTICHEAT_FIR_ORDER] = {
    0.0385f, 0.1095f, 0.2020f, 0.2500f, 0.2020f, 0.1095f, 0.0385f};

static arm_fir_instance_f32 s_fir_instance;
static float32_t s_fir_state[ANTICHEAT_FIR_ORDER + 1];

/*---------------------------------------------------------------------------*/
/* Baseline auto-calibration state                                          */
/*---------------------------------------------------------------------------*/
typedef enum
{
    AC_COLLECTING_BASELINE = 0,
    AC_READY
} AcState;

static AcState s_state = AC_COLLECTING_BASELINE;
static float32_t s_baseline_buf[ANTICHEAT_BASELINE_SAMPLES];
static uint16_t s_baseline_count = 0;
static uint16_t s_warmup_skip = 0;    /* discard first N samples for FIR convergence */
static float32_t s_baseline = 1.0f;   /* default ~1g when stationary */
static float32_t s_threshold = 1.50f; /* will be recalculated after baseline */

/*---------------------------------------------------------------------------*/
/* Peak detection state                                                     */
/*---------------------------------------------------------------------------*/
static float32_t s_prev_mag = 0.0f;
static uint8_t s_peak_rising = 0;
static uint8_t s_wait_reset = 0;
static uint8_t s_dip_seen = 0;        /* 1 after sustained dip confirmed */
static uint8_t s_dip_consecutive = 0; /* consecutive samples below dip level */
static uint32_t s_last_jump_time = 0;
static uint16_t s_gsensor_jump_count = 0;

/*---------------------------------------------------------------------------*/
/* Helpers                                                                  */
/*---------------------------------------------------------------------------*/

static float32_t AC_FilterSample(float32_t input)
{
    float32_t out;
    arm_fir_f32(&s_fir_instance, &input, &out, 1);
    return out;
}

/*---------------------------------------------------------------------------*/
/* Public API                                                               */
/*---------------------------------------------------------------------------*/

void HallAntiCheat_Init(void)
{
    arm_fir_init_f32(&s_fir_instance, ANTICHEAT_FIR_ORDER,
                     (float32_t *)s_fir_coeffs, s_fir_state, 1);
    memset(s_fir_state, 0, sizeof(s_fir_state));

    s_state = AC_COLLECTING_BASELINE;
    s_baseline_count = 0;
    s_warmup_skip = 0;
    s_baseline = 1.0f;
    s_threshold = 1.0f + ANTICHEAT_PEAK_THRESHOLD_G;

    s_prev_mag = 0.0f;
    s_peak_rising = 0;
    s_wait_reset = 0;
    s_dip_seen = 0;
    s_dip_consecutive = 0;
    s_last_jump_time = 0;
    s_gsensor_jump_count = 0;

    DBG_PRINT("[AntiCheat] Module initialized\n");
}

void HallAntiCheat_Process(int16_t *axis)
{
    /* Use the shared magnitude conversion from gsensor module
       to guarantee consistent counts-per-g and FSR handling. */
    float32_t mag = (float32_t)Gsensor_CalcMagnitude_g_from_raw(axis);
    float32_t filtered = AC_FilterSample(mag);

    switch (s_state)
    {
    case AC_COLLECTING_BASELINE:
        /* Skip initial samples while FIR state buffer is still converging */
        if (s_warmup_skip < ANTICHEAT_FIR_WARMUP_SKIP)
        {
            s_warmup_skip++;
            break;
        }

        if (s_baseline_count < ANTICHEAT_BASELINE_SAMPLES)
        {
            s_baseline_buf[s_baseline_count++] = filtered;
        }

        if (s_baseline_count >= ANTICHEAT_BASELINE_SAMPLES)
        {
            /* Compute mean */
            float32_t sum = 0.0f;
            for (uint16_t i = 0; i < ANTICHEAT_BASELINE_SAMPLES; i++)
                sum += s_baseline_buf[i];
            float32_t mean = sum / (float32_t)ANTICHEAT_BASELINE_SAMPLES;

            /* Compute stddev */
            float32_t var = 0.0f;
            for (uint16_t i = 0; i < ANTICHEAT_BASELINE_SAMPLES; i++)
            {
                float32_t d = s_baseline_buf[i] - mean;
                var += d * d;
            }
            float32_t stddev;
            arm_sqrt_f32(var / (float32_t)ANTICHEAT_BASELINE_SAMPLES, &stddev);

            /* Validate: stddev must be low AND mean must be near 1g */
            if (stddev <= ANTICHEAT_BASELINE_STDDEV_G &&
                mean >= ANTICHEAT_BASELINE_MIN_G &&
                mean <= ANTICHEAT_BASELINE_MAX_G)
            {
                /* Baseline locked */
                s_baseline = mean;
                s_threshold = s_baseline + ANTICHEAT_PEAK_THRESHOLD_G;
                s_state = AC_READY;
                s_prev_mag = filtered;
                DBG_PRINT("[AntiCheat] Baseline=%.3fg std=%.4fg Threshold=%.3fg\n",
                          (double)s_baseline, (double)stddev, (double)s_threshold);
            }
            else
            {
                /* Rejected: shift window and keep collecting */
                DBG_PRINT("[AntiCheat] Baseline rejected mean=%.3fg std=%.4fg\n",
                          (double)mean, (double)stddev);
                uint16_t shift = ANTICHEAT_BASELINE_SAMPLES / 2;
                memmove(s_baseline_buf, &s_baseline_buf[shift],
                        (ANTICHEAT_BASELINE_SAMPLES - shift) * sizeof(float32_t));
                s_baseline_count = ANTICHEAT_BASELINE_SAMPLES - shift;
            }
        }
        break;

    case AC_READY:
    {
        uint32_t now = get_ticks_ms();
        float32_t reset_level = s_baseline + ANTICHEAT_RESET_HYSTERESIS_G;
        float32_t dip_level = s_baseline - ANTICHEAT_DIP_BELOW_BASELINE_G;

        /* Track sustained airborne dip: magnitude must stay below dip_level
           for ANTICHEAT_DIP_MIN_SAMPLES consecutive samples.
           Hand-shaking produces brief 1-2 sample dips; real jumping
           keeps magnitude low for 80-200ms (4-10 samples at 50Hz). */
        if (filtered < dip_level)
        {
            s_dip_consecutive++;
            if (s_dip_consecutive >= ANTICHEAT_DIP_MIN_SAMPLES)
            {
                s_dip_seen = 1;
            }
        }
        else
        {
            s_dip_consecutive = 0;
        }

        if (s_wait_reset)
        {
            if (filtered < reset_level)
            {
                s_wait_reset = 0;
            }
        }
        else
        {
            if (s_prev_mag < filtered)
            {
                /* Rising edge */
                s_peak_rising = 1;
            }
            else if (s_peak_rising && s_prev_mag > filtered)
            {
                /* Falling edge after rise — only count if:
                   1) a sustained dip was confirmed (airborne phase)
                   2) peak exceeds dynamic threshold (landing impact) */
                if (s_dip_seen && s_prev_mag > s_threshold)
                {
                    if (get_elapsed_ms(s_last_jump_time) >= ANTICHEAT_MIN_INTERVAL_MS)
                    {
                        s_gsensor_jump_count++;
                        s_last_jump_time = now;
                        s_wait_reset = 1;
                        s_dip_seen = 0;
                        s_dip_consecutive = 0;
                        DBG_PRINT("[AntiCheat] GS jump #%u mag=%.3fg\n",
                                  (unsigned)s_gsensor_jump_count, (double)s_prev_mag);
                    }
                }
                s_peak_rising = 0;
            }
        }

        s_prev_mag = filtered;
        break;
    }
    }
}

void HallAntiCheat_Reset(void)
{
    s_gsensor_jump_count = 0;
    s_prev_mag = 0.0f;
    s_peak_rising = 0;
    s_wait_reset = 0;
    s_dip_seen = 0;
    s_dip_consecutive = 0;
    s_last_jump_time = 0;
    /* Preserve baseline - no need to re-calibrate */
    DBG_PRINT("[AntiCheat] Counter reset (baseline preserved)\n");
}

uint16_t HallAntiCheat_GetGsensorCount(void)
{
    return s_gsensor_jump_count;
}

uint8_t HallAntiCheat_IsReady(void)
{
    return (s_state == AC_READY) ? 1u : 0u;
}

uint16_t HallAntiCheat_ValidateHallTotal(uint16_t hall_raw_total)
{
    if (s_state != AC_READY)
    {
        /* Not calibrated yet — pass through without capping */
        return hall_raw_total;
    }

    uint16_t gs = s_gsensor_jump_count;

    /* Tolerance = max(fixed count, percentage of gsensor) */
    uint16_t ratio_tol = (uint16_t)((float)gs * ANTICHEAT_TOLERANCE_RATIO);
    uint16_t tolerance = (ratio_tol > ANTICHEAT_TOLERANCE_COUNT)
                             ? ratio_tol
                             : ANTICHEAT_TOLERANCE_COUNT;
    uint16_t max_allowed = gs + tolerance;

    /* Overflow guard */
    if (max_allowed < gs)
        max_allowed = 0xFFFFu;

    if (hall_raw_total <= max_allowed)
    {
        return hall_raw_total;
    }

    DBG_PRINT("[AntiCheat] CAPPED hall=%u -> %u (gs=%u tol=%u)\n",
              (unsigned)hall_raw_total, (unsigned)max_allowed,
              (unsigned)gs, (unsigned)tolerance);
    return max_allowed;
}

#endif /* USE_HALL_ANTICHEAT */
