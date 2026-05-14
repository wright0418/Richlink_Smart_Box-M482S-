/* project_config.h - Project-wide configuration and compile-time flags */
#ifndef _PROJECT_CONFIG_H_
#define _PROJECT_CONFIG_H_

/* System/clock configuration */
#define PLL_CLOCK 96000000

#define DEBUG 1 /* Normal run: disable heavy UART debug noise */
#if DEBUG
/* Only include stdio when debug printing is enabled */
#include <stdio.h>
#define DBG_PRINT(fmt, ...) printf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DBG_PRINT(fmt, ...)
#endif

/* I2C failure diagnostics
   1: print concise I2C error snapshots via UART printf (recommended while bring-up)
   0: disable I2C diagnostics */
#define I2C_DIAG_LOG_ENABLE 0

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

/* Firmware version and board identification
   ------------------------------------------------------------------
   FW_VERSION is built from:
     FW_VERSION_MAJOR.FW_VERSION_MINOR.FW_VERSION_PATCH
   The major digit is selected automatically from enabled LED feature
   flags so the version string encodes the capability tier. */
#define FW_VERSION_MINOR 1
#define FW_VERSION_PATCH 0
#define BOARD_NAME "RL_WEAR_V1"
#define FW_BUILD_DATE __DATE__
#define FW_BUILD_TIME __TIME__

#define FW_STR_IMPL(x) #x
#define FW_STR(x) FW_STR_IMPL(x)

/* General project flags */

/* Power lock control
   1: enable PA11 power-lock control path (legacy behavior)
   0: disable PA11 power-lock control and rely on physical power switch */
#define POWER_LOCK_ENABLE 0

/* Cognitive training / Whac-A-Mole firmware profile
   -------------------------------------------------
   This mode keeps the BLE transport and board diagnostics, disables the
   legacy rope-jump gameplay paths, and enables the BLE GATT UART LED matrix
   protocol used by the cognitive training app. */
#define USE_MOLE_GAME 0
#define USE_SQUAT_MODE 1
#define MOLE_TEST_TRACE_ENABLE 1 /* 1: print Mole protocol/hit trace on UART0 for COM3 verification */
/* Separate control for BLE / UART1 related MOLE_TEST prints.
   Default follows MOLE_TEST_TRACE_ENABLE to preserve legacy behavior. */
#ifndef MOLE_TEST_BLE_TRACE_ENABLE
#define MOLE_TEST_BLE_TRACE_ENABLE 0 /* default: disable verbose BLE/UART1 hex dumps; set to 1 for debug */
#endif

/* BLE device name prefixes used by the active firmware profile.
   - USE_MOLE_GAME: rename to MOLE_XXXX
   - USE_SQUAT_MODE: rename to SPORT_XXXX
   - legacy rope mode: rename to ROPE_XXXX */
#define MOLE_BLE_NAME_PREFIX "MOLE_"
#define MOLE_LED_COUNT 256u
#define SPORT_BLE_NAME_PREFIX "SPORT_"
#define MOLE_LED_ROWS 8u
#define MOLE_LED_COLS 8u
#define MOLE_LED_LEGACY8_SUPPORT 1u
#define MOLE_ENABLE_RGB16X16 1u
#define MOLE_ENABLE_RGB16X16_COLOR 1u
#define MOLE_RGB16_ROWS 16u
#define MOLE_RGB16_COLS 16u
#define MOLE_RGB16_LED_COUNT (MOLE_RGB16_ROWS * MOLE_RGB16_COLS)
#define MOLE_RGB16_COLOR_BYTES (MOLE_RGB16_LED_COUNT * 3u)
/* Keep the RGB chunk payload small enough for conservative BLE UART writes. */
#define MOLE_RGB16_CHUNK_PAYLOAD_MAX 10u
#if MOLE_ENABLE_RGB16X16
#define MOLE_WS2812_LED_COUNT MOLE_RGB16_LED_COUNT
#else
#define MOLE_WS2812_LED_COUNT MOLE_LED_COUNT
#endif
#define MOLE_LED_DEFAULT_BRIGHTNESS_PERCENT 10u
#define MOLE_LED_MIN_HOLD_MS 30u /* consecutive LED frames: keep current frame visible for at least this duration */
#define MOLE_WS2812_BOOT_SELF_TEST_BRIGHTNESS_PERCENT 10u
#define MOLE_WS2812_BOOT_SELF_TEST_COLOR_HOLD_MS 2000u
#define MOLE_WS2812_BOOT_SELF_TEST_SENSOR_SAMPLES 10u
#define MOLE_PACKET_RX_CACHE_SIZE 256u
#define MOLE_HIT_BUTTON_ENABLE 1
#define MOLE_HIT_GSENSOR_ENABLE 1
#define MOLE_HIT_SAMPLE_INTERVAL_MS 20u
#define MOLE_HIT_DEBOUNCE_MS 180u
#define MOLE_HIT_MERGE_WINDOW_MS 120u
#define MOLE_HIT_JERK_THRESHOLD_G 0.65f
#define MOLE_HIT_MAG_DELTA_THRESHOLD_G 0.45f
#define MOLE_DISABLE_IDLE_POWER_OFF 1
#define MOLE_DISABLE_USB_CHARGE_LOOP 1
/* ICE 調試期間常出現低壓誤判，暫時停用低電壓自動關機。 */
#define MOLE_LOW_BATT_POWER_OFF 0
#define MOLE_LOW_BATT_SHUTDOWN_CONFIRM_COUNT 3u
#define MOLE_LOW_BATT_SHUTDOWN_GRACE_MS 100u
#define MOLE_WS2812_BOOT_SELF_TEST 1
#define MOLE_WS2812_BOOT_SELF_TEST_STEP_MS 250u
#define MOLE_WS2812_DIAG_REPEAT 0
#define MOLE_WS2812_DIAG_STEP_MS 1000u
#define MOLE_WS2812_DIAG_GPIO_PROBE_MS 200u

/* Squat mode (was 8x8 RGB + G-sensor). For RL穿戴系統 debug, use 16x16 RGB display */
#define SQUAT_USE_RGB_8X8 0
#define SQUAT_USE_CMSIS_DSP 1
#define SQUAT_SAMPLE_RATE_HZ 50u
#define SQUAT_DISPLAY_FRAME_INTERVAL_MS 33u
#define SQUAT_SENSOR_FORCE_MXC400 1
#define SQUAT_ENABLE_REPL_RAW_STREAM 1
#define SQUAT_ENABLE_REPL_FEATURE_STREAM 1
#define SQUAT_ENABLE_REPL_STATE_STREAM 1
#define SQUAT_ENABLE_PROGRESS_BAR 1
#define SQUAT_ENABLE_REP_FLASH 1
#define SQUAT_DISPLAY_MAX_COUNT 99u

#define GSENSOR_FORCE_DEVICE_NONE 0u
#define GSENSOR_FORCE_DEVICE_SC7U22 1u
#define GSENSOR_FORCE_DEVICE_MXC400 2u
#if USE_SQUAT_MODE && SQUAT_SENSOR_FORCE_MXC400
#define GSENSOR_FORCE_DEVICE GSENSOR_FORCE_DEVICE_MXC400
#else
#define GSENSOR_FORCE_DEVICE GSENSOR_FORCE_DEVICE_NONE
#endif

#if USE_SQUAT_MODE && USE_MOLE_GAME
#error "USE_SQUAT_MODE and USE_MOLE_GAME are mutually exclusive"
#endif

/*
 * Auto version major-digit policy (feature-driven):
 * 1.x.y: legacy 8x8 only
 * 2.x.y: 16x16 mono enabled
 * 3.x.y: 16x16 color chunk enabled
 */
#if MOLE_ENABLE_RGB16X16 && MOLE_ENABLE_RGB16X16_COLOR
#define FW_VERSION_MAJOR 0
#elif MOLE_ENABLE_RGB16X16
#define FW_VERSION_MAJOR 2
#else
#define FW_VERSION_MAJOR 1
#endif

#define FW_VERSION FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)

/* Firmware capability bits returned by diagnostic commands.
   FW_CAPABILITY_MASK is assembled from the enabled feature flags and
   is exposed by AT+TEST,VERSION / AT+TEST,CAPABILITIES. */
#define FW_CAP_LEGACY_8X8_MONO 0x00000001u
#define FW_CAP_RGB16_MONO 0x00000002u
#define FW_CAP_RGB16_COLOR_CHUNKED 0x00000004u
#define FW_CAP_BLE_AT_REPL 0x00000008u
#define FW_CAP_HIT_BUTTON 0x00000010u
#define FW_CAP_HIT_GSENSOR 0x00000020u
#define FW_CAP_MOLE_PROFILE 0x00000040u

#define FW_CAPABILITY_MASK_BASE \
   (FW_CAP_LEGACY_8X8_MONO | FW_CAP_MOLE_PROFILE)
#if MOLE_ENABLE_RGB16X16 && MOLE_ENABLE_RGB16X16_COLOR
#define FW_CAPABILITY_MASK_RGB16 (FW_CAP_RGB16_MONO | FW_CAP_RGB16_COLOR_CHUNKED)
#elif MOLE_ENABLE_RGB16X16
#define FW_CAPABILITY_MASK_RGB16 FW_CAP_RGB16_MONO
#else
#define FW_CAPABILITY_MASK_RGB16 0u
#endif
#if USE_BLE_AT_REPL
#define FW_CAPABILITY_MASK_REPL FW_CAP_BLE_AT_REPL
#else
#define FW_CAPABILITY_MASK_REPL 0u
#endif
#if MOLE_HIT_BUTTON_ENABLE
#define FW_CAPABILITY_MASK_HIT_BUTTON FW_CAP_HIT_BUTTON
#else
#define FW_CAPABILITY_MASK_HIT_BUTTON 0u
#endif
#if MOLE_HIT_GSENSOR_ENABLE
#define FW_CAPABILITY_MASK_HIT_GSENSOR FW_CAP_HIT_GSENSOR
#else
#define FW_CAPABILITY_MASK_HIT_GSENSOR 0u
#endif
#define FW_CAPABILITY_MASK                                                         \
   (FW_CAPABILITY_MASK_BASE | FW_CAPABILITY_MASK_RGB16 | FW_CAPABILITY_MASK_REPL | \
    FW_CAPABILITY_MASK_HIT_BUTTON | FW_CAPABILITY_MASK_HIT_GSENSOR)

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
#define BOARD_TEST_AUTORUN 1

/* I2C / G-sensor robustness */
#define GSENSOR_I2C_BUS_HZ 400000u /* Fast-mode I2C (400 kHz) */
#define I2C_XFER_RETRY_COUNT 3u
#define GSENSOR_INIT_RETRY_COUNT 3u
#define GSENSOR_RECOVERY_RETRY_INTERVAL 5u

/* SC7U22 startup timing robustness
   Some boards show delayed sensor ACK after MCU reset/program load.
   These values let init wait for I2C address ACK before WHO_AM_I/config. */
#define GSENSOR_SC7U22_ACK_WAIT_TIMEOUT_MS 300u
#define GSENSOR_SC7U22_ACK_POLL_INTERVAL_MS 10u
#define GSENSOR_SC7U22_INIT_RETRY_DELAY_MS 30u

/* Sensor addresses (7-bit)
   Runtime auto-detect probes SC7U22 first (preferred + alternate), then MXC400. */
#define GSENSOR_MXC400_I2C_ADDR 0x15u

/* SC7U22 I2C address is selected by SDO wiring:
   SDO=Low/GND  -> 7-bit 0x18, bus bytes 0x30(W) / 0x31(R)
   SDO=High/VDD -> 7-bit 0x19, bus bytes 0x32(W) / 0x33(R)
   Nuvoton StdDriver APIs expect the 7-bit slave address.
   This setting is the preferred SC7U22 address; driver will fallback to the
   alternate address automatically when probing. */
#define GSENSOR_SC7U22_I2C_ADDR 0x19u

/* G-Sensor Jump Detection Configuration */
/* Set to 1 to use G-Sensor based jump counting (replaces HALL sensor) */
/* Master switch: enable or disable all jump-detection features (HALL/G-sensor) */
#ifndef USE_JUMP_DETECT
#define USE_JUMP_DETECT 0
#endif

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

/* If the master jump-detection switch is disabled, force-disable both
   G-sensor and Hall-based detection modes and related hit reporting. */
#if !USE_JUMP_DETECT
#undef USE_GSENSOR_JUMP_DETECT
#define USE_GSENSOR_JUMP_DETECT 0
#undef USE_HALL_ANTICHEAT
#define USE_HALL_ANTICHEAT 0
#undef MOLE_HIT_GSENSOR_ENABLE
#define MOLE_HIT_GSENSOR_ENABLE 0
#endif

#if USE_SQUAT_MODE && (USE_GSENSOR_JUMP_DETECT || USE_HALL_ANTICHEAT)
#error "USE_SQUAT_MODE is mutually exclusive with jump/anti-cheat modes"
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
