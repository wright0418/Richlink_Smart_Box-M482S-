/* project_config.h - Project-wide configuration and compile-time flags */
#ifndef _PROJECT_CONFIG_H_
#define _PROJECT_CONFIG_H_

/* System/clock configuration */
#define PLL_CLOCK 192000000

/* Debug logging */
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

/* General project flags */
#define STANDBY_TIME (60 * 1000) /* 60 sec */
#define BUF_SIZE 512			 /* Size of DBG_PRINT buffer */
#define enable_Gsensor_Mode 1

#endif // _PROJECT_CONFIG_H_
