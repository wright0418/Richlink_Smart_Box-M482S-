#ifndef WS2812B_PATTERNS_H
#define WS2812B_PATTERNS_H

#include <stdint.h>

#include "ws2812b.h"

typedef struct
{
    const char *name;
    WS2812B_TemplateId template_id;
    WS2812B_Color on_color;
    WS2812B_Color off_color;
    uint8_t use_polling;
} WS2812B_GameTemplate;

uint8_t WS2812B_GameTemplateCount(void);
const WS2812B_GameTemplate *WS2812B_GameTemplateAt(uint8_t index);

#endif /* WS2812B_PATTERNS_H */
