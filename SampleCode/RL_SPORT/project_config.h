/* project_config.h - Project-wide configuration and compile-time flags */
#ifndef _PROJECT_CONFIG_H_
#define _PROJECT_CONFIG_H_

/* System/clock configuration */
#define PLL_CLOCK 96000000

#define DEBUG 1
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

/* General project flags */

/* Movement-based idle detection configuration
	 - When BLE is connected and device is stationary for NO_MOVEMENT_TIMEOUT_CONNECTED_MS,
		 mark idle state.
	 - When BLE is disconnected and device is stationary for NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS,
		 mark idle state.
*/
#define NO_MOVEMENT_TIMEOUT_CONNECTED_MS (60 * 1000)	/* 60 seconds when BLE connected */
#define NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS (30 * 1000) /* 30 seconds when BLE disconnected */

/* Movement sampling and detection parameters */
#define MOVEMENT_SAMPLE_INTERVAL_MS 500	  /* sample every 500 ms */
#define MOVEMENT_WINDOW_SAMPLES 8		  /* sliding window size */
#define MOVEMENT_STDDEV_THRESHOLD_G 0.02f /* stddev threshold in g to consider 'no movement' */
#define MOVEMENT_MAG_TOLERANCE_G 0.4f	  /* magnitude deviation from 1g considered stable (relaxed for sensor calibration) */

/* Battery ADC configuration (PB1 -> EADC0_CH1)
	Assumptions: Vref=4.0V, divider=1/2 (ADC=BAT/2) */
#define ADC_VREF_V 3.3f
#define ADC_FULL_SCALE 4095.0f
#define ADC_DIVIDER_RATIO 0.5f
#define ADC_BATT_LOW_V 3.0f
#define ADC_BATT_AVG_SAMPLES 4u
#define ADC_CONV_TIMEOUT 10000u
#define LOW_BATT_CHECK_INTERVAL_MS 1000u
#define LOW_BATT_LED_FREQ_HZ 4.0f
#define LOW_BATT_LED_DUTY 0.1f

/* Board test control
	0: Do not run BoardTest_RunAll() at boot (default for normal firmware)
	1: Run BoardTest_RunAll() once at boot */
#define BOARD_TEST_AUTORUN 1

/* I2C / G-sensor robustness */
#define I2C_XFER_RETRY_COUNT 3u
#define GSENSOR_INIT_RETRY_COUNT 3u
#define GSENSOR_RECOVERY_RETRY_INTERVAL 5u

/* G-Sensor Jump Detection Configuration */
/* Set to 1 to use G-Sensor based jump counting (replaces HALL sensor) */
#define USE_GSENSOR_JUMP_DETECT 0

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
#define JUMP_FIR_ORDER 7		  /* Filter order (number of taps) */
#define JUMP_FIR_CUTOFF_HZ 6.0f	  /* Cutoff frequency in Hz */
#define JUMP_SAMPLE_RATE_HZ 50.0f /* G-Sensor sampling rate (50Hz = 20ms period) */

/* Jump Detection Thresholds */
#define JUMP_THRESHOLD_MULTIPLIER 1.5f /* Threshold = baseline + (multiplier * std_dev) */
#define JUMP_MIN_INTERVAL_MS 200	   /* Minimum time between jumps (debounce) */
/* Require signal to drop below baseline + hysteresis before next jump is allowed */
#define JUMP_RESET_HYSTERESIS_G 0.20f /* Hysteresis (in g) to prevent double count on up/down */

/* Calibration Configuration */
#define JUMP_CALIB_STATIC_TIME_MS 2000 /* Static baseline collection time (2 seconds) */
#define JUMP_CALIB_DYNAMIC_JUMPS 8	   /* Number of jumps for dynamic calibration */
#define JUMP_CALIB_TIMEOUT_MS 30000	   /* Maximum calibration time (30 seconds) */
#endif								   /* USE_GSENSOR_JUMP_DETECT */

#endif // _PROJECT_CONFIG_H_
