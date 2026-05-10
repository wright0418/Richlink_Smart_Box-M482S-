#include "ws2812b_digits.h"
#include "timer.h"

#define DIGIT_W 3u
#define DIGIT_H 5u

static const uint8_t s_digit_3x5[10][DIGIT_H] = {
    {0x07u, 0x05u, 0x05u, 0x05u, 0x07u}, /*0*/
    {0x02u, 0x06u, 0x02u, 0x02u, 0x07u}, /*1*/
    {0x07u, 0x01u, 0x07u, 0x04u, 0x07u}, /*2*/
    {0x07u, 0x01u, 0x07u, 0x01u, 0x07u}, /*3*/
    {0x05u, 0x05u, 0x07u, 0x01u, 0x01u}, /*4*/
    {0x07u, 0x04u, 0x07u, 0x01u, 0x07u}, /*5*/
    {0x07u, 0x04u, 0x07u, 0x05u, 0x07u}, /*6*/
    {0x07u, 0x01u, 0x01u, 0x01u, 0x01u}, /*7*/
    {0x07u, 0x05u, 0x07u, 0x05u, 0x07u}, /*8*/
    {0x07u, 0x05u, 0x07u, 0x01u, 0x07u}  /*9*/
};

static WS2812B_Color phase_color(SquatPhase phase)
{
    switch (phase)
    {
    case SQUAT_PHASE_DESCEND:
        return WS2812B_ColorMake(255u, 180u, 0u);
    case SQUAT_PHASE_BOTTOM:
        return WS2812B_ColorMake(255u, 90u, 0u);
    case SQUAT_PHASE_ASCEND:
        return WS2812B_ColorMake(0u, 255u, 0u);
    case SQUAT_PHASE_STAND:
        return WS2812B_ColorMake(0u, 200u, 255u);
    case SQUAT_PHASE_IDLE:
    default:
        return WS2812B_ColorMake(0u, 0u, 16u);
    }
}

void WS2812B_DrawDigit3x5(uint8_t digit, uint8_t x, uint8_t y, WS2812B_Color color)
{
    if (digit > 9u)
    {
        digit = 0u;
    }

    for (uint8_t r = 0u; r < DIGIT_H; r++)
    {
        for (uint8_t c = 0u; c < DIGIT_W; c++)
        {
            uint8_t on = (s_digit_3x5[digit][r] & (uint8_t)(1u << (DIGIT_W - 1u - c))) ? 1u : 0u;
            uint16_t idx = (uint16_t)(y + r) * 8u + (uint16_t)(x + c);
            if ((x + c) < 8u && (y + r) < 8u)
            {
                if (on)
                {
                    WS2812B_SetPixel(idx, color.r, color.g, color.b);
                }
            }
        }
    }
}

void WS2812B_ShowNumber2Digit(uint16_t value, WS2812B_Color color)
{
    uint8_t tens;
    uint8_t ones;
    if (value > 99u)
    {
        value = 99u;
    }

    tens = (uint8_t)(value / 10u);
    ones = (uint8_t)(value % 10u);

    WS2812B_DrawDigit3x5(tens, 0u, 1u, color);
    WS2812B_DrawDigit3x5(ones, 4u, 1u, color);
}

void WS2812B_ShowProgressBar8(uint8_t level, WS2812B_Color color_on, WS2812B_Color color_off)
{
    if (level > 8u)
    {
        level = 8u;
    }

    for (uint8_t c = 0u; c < 8u; c++)
    {
        uint16_t idx = 7u * 8u + c;
        WS2812B_Color cc = (c < level) ? color_on : color_off;
        WS2812B_SetPixel(idx, cc.r, cc.g, cc.b);
    }
}

void WS2812B_ShowSquatScreen(uint16_t count, SquatPhase phase, uint8_t progress)
{
    WS2812B_Color top = phase_color(phase);
    WS2812B_Color num = WS2812B_ColorMake(0u, 220u, 255u);

    WS2812B_Clear();

    for (uint8_t c = 0u; c < 8u; c++)
    {
        WS2812B_SetPixel(c, top.r, top.g, top.b);
    }

    WS2812B_ShowNumber2Digit(count, num);
    WS2812B_ShowProgressBar8(progress, WS2812B_ColorMake(0u, 255u, 0u), WS2812B_ColorMake(0u, 12u, 0u));
    (void)WS2812B_Refresh();
}

void WS2812B_ShowSquatFlash(WS2812B_Color color, uint32_t hold_ms)
{
    WS2812B_FillColor(color);
    (void)WS2812B_Refresh();
    delay_ms(hold_ms);
}
