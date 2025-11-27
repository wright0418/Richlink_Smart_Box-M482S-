/**
 * @file gsensor_jump_detect.c
 * @brief G-Sensor based jump rope detection implementation
 * @version 1.0
 * @date 2025-11-28
 */

#include "gsensor_jump_detect.h"
#include <math.h>
/* Ensure prototype visible to strict linters */
extern float sqrtf(float);
#ifdef ARM_MATH_CM4
/* Try project-relative path first to satisfy static analysis */
#include "../../../Library/CMSIS/Include/arm_math.h"
#endif

/* Fallback types/functions if CMSIS DSP headers are not available to linter */
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

#if USE_GSENSOR_JUMP_DETECT

#include "system_status.h"
#include "timer.h"
#include "buzzer.h"
#include "led.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants and Configuration                                              */
/*---------------------------------------------------------------------------*/

/* FIR Filter Design (Low-pass, Hamming window) */
/* Cutoff = 6Hz, Fs = 50Hz, Order = 7 */
/* Generated using: scipy.signal.firwin(7, 6, fs=50, window='hamming') */
static const float32_t g_fir_coeffs[JUMP_FIR_ORDER] = {
    0.0385f, 0.1095f, 0.2020f, 0.2500f, 0.2020f, 0.1095f, 0.0385f};

/* Sensor sensitivity: MXC4005XC at FSR_2G = 1024 counts/g */
#define GSENSOR_COUNTS_PER_G 1024.0f

/* Maximum samples for calibration buffers */
#define MAX_CALIB_SAMPLES 150 /* 3 seconds at 50Hz */
#define MAX_PEAK_SAMPLES 50   /* Store up to 50 peaks for stats */

/*---------------------------------------------------------------------------*/
/* Module State Variables                                                   */
/*---------------------------------------------------------------------------*/

/* FIR filter instance and state */
static arm_fir_instance_f32 g_fir_instance;
static float32_t g_fir_state[JUMP_FIR_ORDER + 1]; /* State buffer */
static float32_t g_fir_input_buffer;              /* Single sample input */
static float32_t g_fir_output_buffer;             /* Single sample output */

/* Calibration state */
static CalibrationState g_calib_state = CALIB_IDLE;
static CalibrationData g_calib_data = {0};
static uint32_t g_calib_start_time = 0;
static uint32_t g_calib_phase_start_time = 0;

/* Calibration data buffers */
static float32_t g_calib_static_magnitudes[MAX_CALIB_SAMPLES];
static uint16_t g_calib_static_count = 0;
static float32_t g_calib_peak_magnitudes[MAX_PEAK_SAMPLES];
static uint16_t g_calib_peak_count = 0;

/* Jump detection state (runtime) */
static float32_t g_last_magnitude = 0.0f;
static float32_t g_prev_magnitude = 0.0f;
static uint32_t g_last_jump_time = 0;
static uint8_t g_peak_detected = 0; /* Edge detector for local maxima */
static uint8_t g_wait_reset = 0;    /* After counting a jump, wait until signal drops below reset threshold */

/* Calibration-specific detection state (separate from runtime to avoid cross-contamination) */
static float32_t g_calib_prev_magnitude = 0.0f;
static uint8_t g_calib_peak_detected = 0;
static uint8_t g_calib_wait_reset = 0;
static uint32_t g_calib_last_peak_time = 0;

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Calculate 3-axis acceleration magnitude
 * @param axis Raw sensor data [X, Y, Z] in counts
 * @return Magnitude in g units
 */
static float32_t CalculateMagnitude(int16_t *axis)
{
    float32_t x_g = (float32_t)axis[0] / GSENSOR_COUNTS_PER_G;
    float32_t y_g = (float32_t)axis[1] / GSENSOR_COUNTS_PER_G;
    float32_t z_g = (float32_t)axis[2] / GSENSOR_COUNTS_PER_G;

    /* Magnitude = sqrt(x^2 + y^2 + z^2) */
    float32_t mag_squared = x_g * x_g + y_g * y_g + z_g * z_g;
    float32_t magnitude;
    arm_sqrt_f32(mag_squared, &magnitude);

    return magnitude;
}

/**
 * @brief Apply FIR low-pass filter to single sample
 * @param input Input sample
 * @return Filtered output sample
 */
static float32_t FilterSample(float32_t input)
{
    g_fir_input_buffer = input;
    arm_fir_f32(&g_fir_instance, &g_fir_input_buffer, &g_fir_output_buffer, 1);
    return g_fir_output_buffer;
}

/**
 * @brief Calculate mean and standard deviation of float array
 * @param data Input array
 * @param length Array length
 * @param mean Output: mean value
 * @param std_dev Output: standard deviation
 */
static void CalculateStats(const float32_t *data, uint32_t length,
                           float32_t *mean, float32_t *std_dev)
{
    if (length == 0)
    {
        *mean = 0.0f;
        *std_dev = 0.0f;
        return;
    }

    /* arm_mean_f32/arm_var_f32 expect non-const pointer; cast away const intentionally */
    arm_mean_f32((float32_t *)data, length, mean);

    float32_t variance;
    arm_var_f32((float32_t *)data, length, &variance);
    arm_sqrt_f32(variance, std_dev);
}

/**
 * @brief Play buzzer beep(s) for calibration feedback
 * @param beeps Number of beeps (1-3)
 */
static void PlayCalibrationBeeps(uint8_t beeps)
{
    for (uint8_t i = 0; i < beeps; i++)
    {
        BuzzerPlay(2000, 100); /* 2kHz, 100ms */
        if (i < beeps - 1)
        {
            delay_ms(150); /* Gap between beeps */
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                */
/*---------------------------------------------------------------------------*/

void JumpDetect_Init(void)
{
    /* Initialize FIR filter instance */
    arm_fir_init_f32(&g_fir_instance,
                     JUMP_FIR_ORDER,
                     (float32_t *)g_fir_coeffs,
                     g_fir_state,
                     1); /* Block size = 1 (single sample processing) */

    /* Reset state variables */
    memset(g_fir_state, 0, sizeof(g_fir_state));
    g_last_magnitude = 0.0f;
    g_prev_magnitude = 0.0f;
    g_last_jump_time = 0;
    g_peak_detected = 0;
    g_wait_reset = 0;

    /* Initialize calibration data */
    g_calib_state = CALIB_IDLE;
    g_calib_data.is_valid = 0;
    g_calib_data.baseline_magnitude = 0.0f;
    g_calib_data.dynamic_threshold = 0.0f;
    g_calib_data.peak_std_dev = 0.0f;

    DBG_PRINT("[JumpDetect] Module initialized\n");
}

void JumpDetect_StartCalibration(void)
{
    /* Reset calibration state */
    g_calib_state = CALIB_STATIC_COLLECTING;
    g_calib_start_time = get_ticks_ms();
    g_calib_phase_start_time = get_ticks_ms();
    g_calib_static_count = 0;
    g_calib_peak_count = 0;
    g_calib_data.is_valid = 0;

    /* Clear all detection states to prevent carry-over */
    g_peak_detected = 0;
    g_wait_reset = 0;
    g_prev_magnitude = 0.0f;
    g_last_jump_time = 0;
    g_calib_prev_magnitude = 0.0f;
    g_calib_peak_detected = 0;
    g_calib_wait_reset = 0;
    g_calib_last_peak_time = 0;

    /* Single beep: Start static calibration */
    PlayCalibrationBeeps(1);

    /* Visual feedback: fast blink during static calibration (5Hz) */
    SetGreenLedMode(5, 0.5);

    DBG_PRINT("[JumpDetect] Calibration started - Hold still for %d seconds\n",
              JUMP_CALIB_STATIC_TIME_MS / 1000);
}

CalibrationState JumpDetect_ProcessCalibration(int16_t *axis)
{
    uint32_t now = get_ticks_ms();
    float32_t magnitude;
    float32_t filtered_magnitude;

    /* Check for timeout */
    if (get_elapsed_ms(g_calib_start_time) > JUMP_CALIB_TIMEOUT_MS)
    {
        g_calib_state = CALIB_TIMEOUT;
        DBG_PRINT("[JumpDetect] Calibration timeout\n");
        PlayCalibrationBeeps(1); /* Error beep */
        delay_ms(200);
        PlayCalibrationBeeps(1);

        /* Visual feedback: rapid flash for error (20Hz) */
        SetGreenLedMode(20, 0.5);
        delay_ms(2000);
        SetGreenLedMode(0.5, 0.5); /* Return to idle blink */

        return g_calib_state;
    }

    /* Calculate magnitude */
    magnitude = CalculateMagnitude(axis);
    filtered_magnitude = FilterSample(magnitude);

    switch (g_calib_state)
    {

    case CALIB_STATIC_COLLECTING:
        /* Collect static baseline samples */
        if (g_calib_static_count < MAX_CALIB_SAMPLES)
        {
            g_calib_static_magnitudes[g_calib_static_count++] = filtered_magnitude;
        }

        /* Check if collection time elapsed */
        if (get_elapsed_ms(g_calib_phase_start_time) >= JUMP_CALIB_STATIC_TIME_MS)
        {
            /* Calculate baseline */
            float32_t mean, std_dev;
            CalculateStats(g_calib_static_magnitudes, g_calib_static_count, &mean, &std_dev);
            g_calib_data.baseline_magnitude = mean;

            DBG_PRINT("[JumpDetect] Static baseline collected: %.3fg (std: %.3fg)\n",
                      mean, std_dev);

            /* Double beep: Start dynamic calibration */
            PlayCalibrationBeeps(2);

            /* Visual feedback: very fast blink during dynamic calibration (10Hz) */
            SetGreenLedMode(10, 0.5);

            g_calib_state = CALIB_DYNAMIC_COLLECTING;
            g_calib_phase_start_time = get_ticks_ms();
            g_calib_prev_magnitude = filtered_magnitude;
            g_calib_peak_detected = 0;
            g_calib_wait_reset = 0;
            g_calib_last_peak_time = 0;
        }
        break;

    case CALIB_DYNAMIC_COLLECTING:
    {
        float32_t reset_threshold = g_calib_data.baseline_magnitude + JUMP_RESET_HYSTERESIS_G;

        if (g_calib_wait_reset)
        {
            if (filtered_magnitude < reset_threshold)
            {
                g_calib_wait_reset = 0;
            }
        }
        else
        {
            if (g_calib_prev_magnitude < filtered_magnitude)
            {
                /* Rising edge */
                g_calib_peak_detected = 1;
            }
            else if (g_calib_peak_detected && g_calib_prev_magnitude > filtered_magnitude)
            {
                /* Falling edge - potential peak */
                if (g_calib_prev_magnitude > (g_calib_data.baseline_magnitude + 0.3f))
                {
                    /* Check debounce: first peak or sufficient time elapsed */
                    uint8_t debounce_pass = (g_calib_last_peak_time == 0) ||
                                            (get_elapsed_ms(g_calib_last_peak_time) >= JUMP_MIN_INTERVAL_MS);

                    if (debounce_pass)
                    {
                        if (g_calib_peak_count < MAX_PEAK_SAMPLES)
                        {
                            g_calib_peak_magnitudes[g_calib_peak_count++] = g_calib_prev_magnitude;
                            DBG_PRINT("[JumpDetect] Peak #%d detected: %.3fg\n",
                                      g_calib_peak_count, g_calib_prev_magnitude);
                        }

                        g_calib_last_peak_time = now;
                        g_calib_wait_reset = 1;
                    }
                }

                g_calib_peak_detected = 0;
            }
        }

        g_calib_prev_magnitude = filtered_magnitude;

        /* Check if enough jumps collected */
        if (g_calib_peak_count >= JUMP_CALIB_DYNAMIC_JUMPS)
        {
            /* Calculate dynamic threshold */
            float32_t peak_mean, peak_std;
            CalculateStats(g_calib_peak_magnitudes, g_calib_peak_count, &peak_mean, &peak_std);

            /* Threshold = baseline + multiplier * std_dev */
            g_calib_data.dynamic_threshold = g_calib_data.baseline_magnitude +
                                             (JUMP_THRESHOLD_MULTIPLIER * peak_std);
            g_calib_data.peak_std_dev = peak_std;

            DBG_PRINT("[JumpDetect] Calibration complete!\n");
            DBG_PRINT("  Baseline: %.3fg\n", g_calib_data.baseline_magnitude);
            DBG_PRINT("  Peak mean: %.3fg, std: %.3fg\n", peak_mean, peak_std);
            DBG_PRINT("  Threshold: %.3fg\n", g_calib_data.dynamic_threshold);

            /* Triple beep: Calibration complete */
            PlayCalibrationBeeps(3);

            /* Visual feedback: solid ON for 1 second to indicate success */
            SetGreenLedMode(0, 0); /* Stop blinking */
            SetGreenLed(1);        /* Turn on solid */
            delay_ms(1000);
            SetGreenLedMode(1, 0.1); /* Return to slow blink (ready state) */

            /* CRITICAL: Reset all detection states to prevent residual from calibration */
            g_peak_detected = 0;
            g_wait_reset = 0;
            g_prev_magnitude = filtered_magnitude;
            g_last_jump_time = get_ticks_ms(); /* Prevent immediate trigger after calibration */
            g_calib_peak_detected = 0;
            g_calib_wait_reset = 0;
            g_calib_prev_magnitude = filtered_magnitude;
            g_calib_last_peak_time = 0;

            /* Change state first, then mark as valid to prevent premature detection */
            g_calib_state = CALIB_COMPLETED;
            g_calib_data.is_valid = 1;
        }
        break;
    }

    default:
        /* No action needed for other states */
        break;
    }

    return g_calib_state;
}

uint8_t JumpDetect_IsCalibrating(void)
{
    return (g_calib_state == CALIB_STATIC_COLLECTING ||
            g_calib_state == CALIB_DYNAMIC_COLLECTING);
}

CalibrationState JumpDetect_GetCalibrationState(void)
{
    return g_calib_state;
}

const CalibrationData *JumpDetect_GetCalibrationData(void)
{
    return &g_calib_data;
}

void JumpDetect_Process(int16_t *axis)
{
    /* Only process if calibrated */
    if (!g_calib_data.is_valid)
    {
        return;
    }

    uint32_t now = get_ticks_ms();

    /* Calculate and filter magnitude */
    float32_t magnitude = CalculateMagnitude(axis);
    float32_t filtered_magnitude = FilterSample(magnitude);

    /* Enforce reset after a jump: must drop below baseline + hysteresis before counting next */
    float32_t reset_threshold = g_calib_data.baseline_magnitude + JUMP_RESET_HYSTERESIS_G;

    if (g_wait_reset)
    {
        if (filtered_magnitude < reset_threshold)
        {
            /* Signal sufficiently dropped; allow next jump */
            g_wait_reset = 0;
        }
    }
    else
    {
        /* Peak detection: count ONLY one peak per cycle on falling edge above dynamic threshold */
        if (g_prev_magnitude < filtered_magnitude)
        {
            /* Rising edge - potential peak forming */
            g_peak_detected = 1;
        }
        else if (g_peak_detected && g_prev_magnitude > filtered_magnitude)
        {
            /* Falling edge - check if peak exceeds threshold */
            if (g_prev_magnitude > g_calib_data.dynamic_threshold)
            {
                /* Valid peak detected - check debounce interval */
                if (get_elapsed_ms(g_last_jump_time) >= JUMP_MIN_INTERVAL_MS)
                {
                    /* Register jump */
                    Sys_IncrementJumpTimes();
                    g_last_jump_time = now;

                    /* After counting, require reset below baseline+hysteresis before next */
                    g_wait_reset = 1;

                    /* Visual feedback: brief flash on each jump */
                    SetGreenLed(1);

                    DBG_PRINT("[JumpDetect] Jump! mag=%.3fg, count=%d\n",
                              g_prev_magnitude, Sys_GetJumpTimes());
                }
            }
            g_peak_detected = 0;
        }
    }

    /* Update state */
    g_prev_magnitude = filtered_magnitude;
    g_last_magnitude = filtered_magnitude;
}

void JumpDetect_Reset(void)
{
    /* Clear FIR filter state */
    memset(g_fir_state, 0, sizeof(g_fir_state));

    /* Reset detection state */
    g_last_magnitude = 0.0f;
    g_prev_magnitude = 0.0f;
    g_last_jump_time = 0;
    g_peak_detected = 0;
    g_wait_reset = 0;

    DBG_PRINT("[JumpDetect] Detection state reset\n");
}

float JumpDetect_GetLastMagnitude(void)
{
    return g_last_magnitude;
}

uint8_t JumpDetect_IsReady(void)
{
    /* Only ready when calibration is valid AND fully completed (not during calibration) */
    return (g_calib_data.is_valid && g_calib_state == CALIB_COMPLETED);
}

#endif /* USE_GSENSOR_JUMP_DETECT */
