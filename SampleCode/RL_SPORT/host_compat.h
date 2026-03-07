/* host_compat.h
 * Lightweight compatibility/shim for static analysis or host builds when
 * the MCU device headers (NuMicro.h) and standard C headers are not
 * available in the analysis environment. This file is intentionally
 * minimal and guarded so it won't interfere when the real headers exist.
 */
#ifndef _HOST_COMPAT_H_
#define _HOST_COMPAT_H_

#if defined(__has_include)
#if __has_include("NuMicro.h")
/* Real device headers available - do nothing. */
#else
/* Provide minimal replacements for symbols used by several modules. */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

/* Minimal C library prototypes used by the code base. These are only
   declared so static analysis doesn't emit implicit-declaration errors. */
char *strstr(const char *haystack, const char *needle);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);

/* Minimal UART stub types and macros used by BLE module. Real device
   headers will provide real definitions on target builds. */
typedef struct { volatile uint32_t INTSTS; } UART_T;
extern UART_T *UART1;

#define UART_INTSTS_RDAIF_Msk  (0x01u)
#define UART_INTSTS_THREIF_Msk (0x02u)
#define UART_INTEN_THREIEN_Msk (0x04u)

/* Helper stubs to satisfy static analysis */
#define UART_IS_RX_READY(u) (0)
#define UART_READ(u) (0)
static inline void UART_DisableInt(UART_T *u, unsigned int m) { (void)u; (void)m; }
static inline void UART_Write(UART_T *u, const void *buf, int len) { (void)u; (void)buf; (void)len; }

#endif
#else
/* No __has_include - provide minimal declarations unconditionally to help
   static analysis in simple host environments. */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
char *strstr(const char *haystack, const char *needle);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
typedef struct { volatile uint32_t INTSTS; } UART_T;
extern UART_T *UART1;
#define UART_INTSTS_RDAIF_Msk  (0x01u)
#define UART_INTSTS_THREIF_Msk (0x02u)
#define UART_INTEN_THREIEN_Msk (0x04u)
#define UART_IS_RX_READY(u) (0)
#define UART_READ(u) (0)
static inline void UART_DisableInt(UART_T *u, unsigned int m) { (void)u; (void)m; }
static inline void UART_Write(UART_T *u, const void *buf, int len) { (void)u; (void)buf; (void)len; }
#endif

#endif /* _HOST_COMPAT_H_ */
