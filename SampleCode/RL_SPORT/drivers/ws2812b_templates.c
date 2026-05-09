#include "ws2812b_templates.h"

#include "../project_config.h"

#include <stddef.h>

static const uint8_t s_template_clear[MOLE_LED_ROWS] = {
    0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u};

static const uint8_t s_template_full[MOLE_LED_ROWS] = {
    0xFFu, 0xFFu, 0xFFu, 0xFFu,
    0xFFu, 0xFFu, 0xFFu, 0xFFu};

static const uint8_t s_template_checkerboard[MOLE_LED_ROWS] = {
    0xAAu, 0x55u, 0xAAu, 0x55u,
    0xAAu, 0x55u, 0xAAu, 0x55u};

static const uint8_t s_template_border[MOLE_LED_ROWS] = {
    0xFFu, 0x81u, 0x81u, 0x81u,
    0x81u, 0x81u, 0x81u, 0xFFu};

static const uint8_t s_template_cross[MOLE_LED_ROWS] = {
    0x81u, 0x42u, 0x24u, 0x18u,
    0x18u, 0x24u, 0x42u, 0x81u};

static const uint8_t s_template_diagonal[MOLE_LED_ROWS] = {
    0x80u, 0x40u, 0x20u, 0x10u,
    0x08u, 0x04u, 0x02u, 0x01u};

const uint8_t *WS2812B_GetTemplateRows(WS2812B_TemplateId template_id, uint8_t *out_row_count)
{
    if (out_row_count != NULL)
    {
        *out_row_count = MOLE_LED_ROWS;
    }

    switch (template_id)
    {
    case WS2812B_TEMPLATE_CLEAR:
        return s_template_clear;
    case WS2812B_TEMPLATE_FULL:
        return s_template_full;
    case WS2812B_TEMPLATE_CHECKERBOARD:
        return s_template_checkerboard;
    case WS2812B_TEMPLATE_BORDER:
        return s_template_border;
    case WS2812B_TEMPLATE_CROSS:
        return s_template_cross;
    case WS2812B_TEMPLATE_DIAGONAL:
        return s_template_diagonal;
    default:
        return s_template_clear;
    }
}
