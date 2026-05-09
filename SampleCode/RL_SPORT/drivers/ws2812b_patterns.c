#include "ws2812b_patterns.h"

#define ARRAY_COUNT(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

static const WS2812B_GameTemplate s_game_templates[] = {
    {"full_red", WS2812B_TEMPLATE_FULL, {255u, 0u, 0u}, {0u, 0u, 0u}, 1u},
    {"checker_green_blue", WS2812B_TEMPLATE_CHECKERBOARD, {0u, 255u, 0u}, {0u, 0u, 255u}, 1u},
    {"border_yellow", WS2812B_TEMPLATE_BORDER, {255u, 255u, 0u}, {0u, 0u, 0u}, 1u},
    {"cross_purple", WS2812B_TEMPLATE_CROSS, {128u, 0u, 128u}, {0u, 0u, 0u}, 1u},
    {"diag_cyan", WS2812B_TEMPLATE_DIAGONAL, {0u, 255u, 255u}, {0u, 0u, 0u}, 1u},
    {"clear", WS2812B_TEMPLATE_CLEAR, {0u, 0u, 0u}, {0u, 0u, 0u}, 1u},
};

uint8_t WS2812B_GameTemplateCount(void)
{
    return ARRAY_COUNT(s_game_templates);
}

const WS2812B_GameTemplate *WS2812B_GameTemplateAt(uint8_t index)
{
    if (index >= WS2812B_GameTemplateCount())
    {
        return (const WS2812B_GameTemplate *)0;
    }

    return &s_game_templates[index];
}
