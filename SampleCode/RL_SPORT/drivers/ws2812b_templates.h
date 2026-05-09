#ifndef WS2812B_TEMPLATES_H
#define WS2812B_TEMPLATES_H

#include <stdint.h>

#include "ws2812b.h"

const uint8_t *WS2812B_GetTemplateRows(WS2812B_TemplateId template_id, uint8_t *out_row_count);

#endif /* WS2812B_TEMPLATES_H */
