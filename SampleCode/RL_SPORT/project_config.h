/* project_config.h - Project-wide configuration and compile-time flags */
#ifndef _PROJECT_CONFIG_H_
#define _PROJECT_CONFIG_H_

/* System/clock configuration */
#define PLL_CLOCK 96000000

#define DEBUG 0
#if DEBUG
/* Only include stdio when debug printing is enabled */
#include <stdio.h>
#define DBG_PRINT(fmt, ...) printf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DBG_PRINT(fmt, ...)
#endif

/* Common boolean-like macros */
#define ON 1
#define OFF 0

/* Portable INLINE / FORCE_INLINE macros
   - INLINE: suggestion for inline where supported
   - FORCE_INLINE: strong inline (attributes) when supported
   - STATIC_INLINE: guarantees internal linkage (static) */
#ifndef INLINE
/* Prefer 'static inline' when compiling with C99 or common compilers.
   Fall back to 'static' if none of the compiler/features are detected. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
/* C99 or later */
#define INLINE static inline
#define FORCE_INLINE static inline
#elif defined(__GNUC__) || defined(__clang__)
#define INLINE static inline
#define FORCE_INLINE static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define INLINE static __inline
#define FORCE_INLINE static __forceinline
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define INLINE static __inline
#define FORCE_INLINE static __forceinline
#elif defined(__ICCARM__)
#define INLINE static inline
#define FORCE_INLINE static inline
#else
#define INLINE static
#define FORCE_INLINE static
#endif
#endif

#ifndef STATIC_INLINE
#define STATIC_INLINE INLINE
#endif

/* Firmware version and board identification */
#define FW_VERSION "1.3.0"
#define BOARD_NAME "RL_SPORT_V3"
#define FW_BUILD_DATE __DATE__
#define FW_BUILD_TIME __TIME__

/* General project flags */

/* Movement-based idle detection configuration
    - When BLE is connected and device is stationary for NO_MOVEMENT_TIMEOUT_CONNECTED_MS,
       mark idle state.
    - When BLE is disconnected and device is stationary for NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS,
       mark idle state.
*/
#define NO_MOVEMENT_TIMEOUT_CONNECTED_MS (60 * 1000)    /* 60 seconds when BLE connected */
#define NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS (30 * 1000) /* 30 seconds when BLE disconnected */

/* Movement sampling and detection parameters */
#define MOVEMENT_SAMPLE_INTERVAL_MS 500 /* sample every 500 ms */
#define MOVEMENT_WINDOW_SAMPLES 8       /* sliding window size */
/* stddev is the primary (and only effective) motion indicator;
   empirical stationary noise reaches ~0.027g, so 0.03f gives safe margin. */
#define MOVEMENT_STDDEV_THRESHOLD_G 0.03f /* stddev threshold in g */
/* Magnitude condition is unreliable (axis sensitivity varies by orientation/FSR).
   Set to 1.0f to effectively disable it: |mean-1g| cannot physically exceed 1.0. */
#define MOVEMENT_MAG_TOLERANCE_G 1.00f /* disabled: magnitude check not used for idle detection */

/* Set to 1 to print movement detection values every sample (for threshold tuning) */
#define MOVEMENT_DEBUG 0

/* Battery / low-voltage detection via VDDA (band-gap method)
   ---------------------------------------------------------------
   AVDD is produced by an LDO from VBAT. When VBAT < 3.3V the LDO
   saturates and AVDD tracks VBAT directly.
   Instead of reading an external ADC pin we measure AVDD itself via
   the internal band-gap (EADC sample module 16, typ 1.20V):
     AVDD_actual = VBG_NOMINAL * 4095 / raw_bg
   Low-voltage condition: AVDD < ADC_VDDA_LOW_V -> trigger LED warning. */
#define ADC_VBG_NOMINAL 1.20f /* M480 internal band-gap typ 1.20V; calibrate if needed */
#define ADC_FULL_SCALE 4095.0f
#define ADC_CONV_TIMEOUT 10000u
#define ADC_VDDA_LOW_V 3.15f /* VDDA threshold for low-battery warning */
#define LOW_BATT_CHECK_INTERVAL_MS 1000u
#define LOW_BATT_LED_FREQ_HZ 4.0f
#define LOW_BATT_LED_DUTY 0.1f

/* Board test control
   0: Do not run BoardTest_RunAll() at boot (default for normal firmware)
   1: Run BoardTest_RunAll() once at boot */
#define BOARD_TEST_AUTORUN 0

/* I2C / G-sensor robustness */
#define I2C_XFER_RETRY_COUNT 3u
#define GSENSOR_INIT_RETRY_COUNT 3u
#define GSENSOR_RECOVERY_RETRY_INTERVAL 5u

/* G-Sensor Jump Detection Configuration */
/* Set to 1 to use G-Sensor based jump counting (replaces HALL sensor) */
#define USE_GSENSOR_JUMP_DETECT 0

/* Hall anti-cheat: validate Hall count against G-sensor estimate.
   When enabled, the G-sensor runs in parallel (FIR + peak detection via
   CMSIS DSP) to independently estimate jumps.  If the Hall count exceeds
   the G-sensor estimate by more than the configured tolerance the excess
   is rejected, preventing cheating by manual magnet rotation.
   Mutually exclusive with USE_GSENSOR_JUMP_DETECT. */
#define USE_HALL_ANTICHEAT 0

#if USE_HALL_ANTICHEAT && USE_GSENSOR_JUMP_DETECT
#error "USE_HALL_ANTICHEAT and USE_GSENSOR_JUMP_DETECT are mutually exclusive"
#endif

/* BLE AT REPL test mode (isolated from game BLE protocol) */
#define USE_BLE_AT_REPL 1

/* Delayed stationary calibration configuration */
#define GS_CAL_START_DELAY_MS 3000u
#define GS_CAL_STABLE_REQUIRED_MS 800u
#define GS_CAL_STABLE_SAMPLE_INTERVAL_MS 100u
#define GS_CAL_STABLE_WINDOW_SAMPLES 8u
#define GS_CAL_STABLE_STDDEV_THRESHOLD_G 0.02f
#define GS_CAL_STABLE_MAG_TOLERANCE_G 0.20f
#define GS_CAL_LED_UNCAL_FREQ_HZ 0.5f /* 2 seconds blink */
#define GS_CAL_LED_CAL_FREQ_HZ 0.25f  /* 4 seconds blink */
#define GS_CAL_LED_DUTY 0.05f

#if USE_GSENSOR_JUMP_DETECT
/* FIR Low-pass Filter Configuration */
#define JUMP_FIR_ORDER 7          /* Filter order (number of taps) */
#define JUMP_FIR_CUTOFF_HZ 6.0f   /* Cutoff frequency in Hz */
#define JUMP_SAMPLE_RATE_HZ 50.0f /* G-Sensor sampling rate (50Hz = 20ms period) */

/* Jump Detection Thresholds */
#define JUMP_THRESHOLD_MULTIPLIER 1.5f /* Threshold = baseline + (multiplier * std_dev) */
#define JUMP_MIN_INTERVAL_MS 200       /* Minimum time between jumps (debounce) */
/* Require signal to drop below baseline + hysteresis before next jump is allowed */
#define JUMP_RESET_HYSTERESIS_G 0.20f /* Hysteresis (in g) to prevent double count on up/down */

/* Calibration Configuration */
#define JUMP_CALIB_STATIC_TIME_MS 2000 /* Static baseline collection time (2 seconds) */
#define JUMP_CALIB_DYNAMIC_JUMPS 8     /* Number of jumps for dynamic calibration */
#define JUMP_CALIB_TIMEOUT_MS 30000    /* Maximum calibration time (30 seconds) */
#endif                                 /* USE_GSENSOR_JUMP_DETECT */

#if USE_HALL_ANTICHEAT
/* FIR low-pass filter (Hamming, same design as jump detect) */
#define ANTICHEAT_FIR_ORDER 7          /* Filter taps */
#define ANTICHEAT_SAMPLE_RATE_HZ 50.0f /* G-sensor poll rate (50Hz = 20ms) */

/* Peak detection — requires a real jump pattern:
   1) Magnitude must DIP below baseline for a sustained duration (airborne phase)
   2) Then a peak above baseline + threshold (landing impact)
   Hand-shaking produces brief dips (~1 sample); real jumping produces
   sustained low-g for 100-200ms (5-10 samples at 50Hz). */
#define ANTICHEAT_PEAK_THRESHOLD_G 0.60f     /* Landing peak must exceed baseline + this (g) */
#define ANTICHEAT_DIP_BELOW_BASELINE_G 0.20f /* Airborne dip: magnitude must drop below baseline - this (g) */
#define ANTICHEAT_DIP_MIN_SAMPLES 4          /* Dip must persist for N consecutive samples (~80ms at 50Hz) */
#define ANTICHEAT_MIN_INTERVAL_MS 280        /* Debounce between jumps (ms); ~3.5 jumps/sec max */
#define ANTICHEAT_RESET_HYSTERESIS_G 0.25f   /* Signal must drop this close to baseline before next peak */

/* Auto baseline collection
   NOTE: The MXC4005 magnitude reported by Gsensor_CalcMagnitude_g_from_raw()
   may read ~0.65g when stationary (1g physical) due to register byte-order
   and FSR calibration.  The MIN/MAX range must accommodate this. */
#define ANTICHEAT_BASELINE_SAMPLES 100    /* 2 seconds at 50Hz */
#define ANTICHEAT_BASELINE_STDDEV_G 0.04f /* Max stddev to accept as stable baseline */
#define ANTICHEAT_BASELINE_MIN_G 0.40f    /* Reject baseline if mean < this (sensor error / no gravity) */
#define ANTICHEAT_BASELINE_MAX_G 1.30f    /* Reject baseline if mean > this */
#define ANTICHEAT_FIR_WARMUP_SKIP 14      /* Discard first N samples for FIR state convergence */

/* Validation tolerance: accepted = min(hall, gsensor + tolerance)
   tolerance = max(TOLERANCE_COUNT, gsensor * TOLERANCE_RATIO) */
#define ANTICHEAT_TOLERANCE_COUNT 3u    /* Minimum absolute tolerance (jumps) */
#define ANTICHEAT_TOLERANCE_RATIO 0.30f /* Percentage tolerance (30%) */
#endif                                  /* USE_HALL_ANTICHEAT */

#endif // _PROJECT_CONFIG_H_
