#include "ws2812b_digits.h"
#include "timer.h"
#include "../project_config.h"

#define DIGIT_W 3u
#define DIGIT_H 5u

#define DIGIT16_W 5u
#define DIGIT16_H 7u

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

static const uint8_t s_digit_5x7[10][DIGIT16_H] = {
    {0x0Eu, 0x11u, 0x13u, 0x15u, 0x19u, 0x11u, 0x0Eu}, /* 0 */
    {0x04u, 0x0Cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x0Eu}, /* 1 */
    {0x0Eu, 0x11u, 0x01u, 0x02u, 0x04u, 0x08u, 0x1Fu}, /* 2 */
    {0x1Eu, 0x01u, 0x01u, 0x0Eu, 0x01u, 0x01u, 0x1Eu}, /* 3 */
    {0x02u, 0x06u, 0x0Au, 0x12u, 0x1Fu, 0x02u, 0x02u}, /* 4 */
    {0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x01u, 0x01u, 0x1Eu}, /* 5 */
    {0x06u, 0x08u, 0x10u, 0x1Eu, 0x11u, 0x11u, 0x0Eu}, /* 6 */
    {0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x08u, 0x08u}, /* 7 */
    {0x0Eu, 0x11u, 0x11u, 0x0Eu, 0x11u, 0x11u, 0x0Eu}, /* 8 */
    {0x0Eu, 0x11u, 0x11u, 0x0Fu, 0x01u, 0x02u, 0x0Cu}  /* 9 */
};

static uint8_t ws_matrix_cols(void)
{
#if MOLE_ENABLE_RGB16X16
    return (uint8_t)MOLE_RGB16_COLS;
#else
    return 8u;
#endif
}

static uint8_t ws_matrix_rows(void)
{
#if MOLE_ENABLE_RGB16X16
    return (uint8_t)MOLE_RGB16_ROWS;
#else
    return 8u;
#endif
}

static void WS2812B_SetPixelXY(uint8_t x, uint8_t y, WS2812B_Color color)
{
    uint8_t cols = ws_matrix_cols();
    uint8_t rows = ws_matrix_rows();

    if ((x >= cols) || (y >= rows))
    {
        return;
    }

    WS2812B_SetPixel((uint16_t)y * (uint16_t)cols + (uint16_t)x, color.r, color.g, color.b);
}

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
    uint8_t cols = ws_matrix_cols();
    uint8_t rows = ws_matrix_rows();

    if (digit > 9u)
    {
        digit = 0u;
    }

    for (uint8_t r = 0u; r < DIGIT_H; r++)
    {
        for (uint8_t c = 0u; c < DIGIT_W; c++)
        {
            uint8_t on = (s_digit_3x5[digit][r] & (uint8_t)(1u << (DIGIT_W - 1u - c))) ? 1u : 0u;
            if ((x + c) < cols && (y + r) < rows)
            {
                if (on)
                {
                    WS2812B_SetPixelXY((uint8_t)(x + c), (uint8_t)(y + r), color);
                }
            }
        }
    }
}

static void WS2812B_DrawDigit5x7(uint8_t digit, uint8_t x, uint8_t y, WS2812B_Color color)
{
    uint8_t cols = ws_matrix_cols();
    uint8_t rows = ws_matrix_rows();

    if (digit > 9u)
    {
        digit = 0u;
    }

    for (uint8_t r = 0u; r < DIGIT16_H; r++)
    {
        for (uint8_t c = 0u; c < DIGIT16_W; c++)
        {
            uint8_t on = (s_digit_5x7[digit][r] & (uint8_t)(1u << (DIGIT16_W - 1u - c))) ? 1u : 0u;
            if ((x + c) < cols && (y + r) < rows)
            {
                if (on)
                {
                    WS2812B_SetPixelXY((uint8_t)(x + c), (uint8_t)(y + r), color);
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
    uint8_t cols = ws_matrix_cols();
    uint8_t rows = ws_matrix_rows();

    if (level > 8u)
    {
        level = 8u;
    }

    if (cols <= 8u)
    {
        for (uint8_t c = 0u; c < 8u; c++)
        {
            WS2812B_Color cc = (c < level) ? color_on : color_off;
            WS2812B_SetPixelXY(c, (uint8_t)(rows - 1u), cc);
        }
    }
    else
    {
        uint8_t level16 = (uint8_t)(level * 2u);
        if (level16 > cols)
        {
            level16 = cols;
        }

        for (uint8_t c = 0u; c < cols; c++)
        {
            WS2812B_Color cc = (c < level16) ? color_on : color_off;
            WS2812B_SetPixelXY(c, (uint8_t)(rows - 1u), cc);
        }
    }
}

void WS2812B_ShowSquatScreen(uint16_t count, SquatPhase phase, uint8_t progress)
{
    WS2812B_Color top = phase_color(phase);
    WS2812B_Color num = WS2812B_ColorMake(0u, 220u, 255u);
    uint8_t cols = ws_matrix_cols();
    uint8_t rows = ws_matrix_rows();

    WS2812B_Clear();

    for (uint8_t c = 0u; c < cols; c++)
    {
        WS2812B_SetPixelXY(c, 0u, top);
    }

    if (cols >= 16u && rows >= 16u)
    {
        uint8_t tens;
        uint8_t ones;

        if (count > 99u)
        {
            count = 99u;
        }

        tens = (uint8_t)(count / 10u);
        ones = (uint8_t)(count % 10u);

        /* 16x16: 5x7 雙位數置中顯示 (x:2..6, x:9..13 / y:4..10) */
        WS2812B_DrawDigit5x7(tens, 2u, 4u, num);
        WS2812B_DrawDigit5x7(ones, 9u, 4u, num);
    }
    else
    {
        WS2812B_ShowNumber2Digit(count, num);
    }

    WS2812B_ShowProgressBar8(progress, WS2812B_ColorMake(0u, 255u, 0u), WS2812B_ColorMake(0u, 12u, 0u));
    (void)WS2812B_Refresh();
}

void WS2812B_ShowSquatFlash(WS2812B_Color color, uint32_t hold_ms)
{
    WS2812B_FillColor(color);
    (void)WS2812B_Refresh();
    delay_ms(hold_ms);
}
