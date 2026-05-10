#ifndef WS2812B_H
#define WS2812B_H

#include <stdint.h>

#include "../protocol/mole_packet.h"

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} WS2812B_Color;

typedef enum
{
    WS2812B_PALETTE_OFF = 0,
    WS2812B_PALETTE_RED = 1,
    WS2812B_PALETTE_GREEN = 2,
    WS2812B_PALETTE_BLUE = 3,
    WS2812B_PALETTE_YELLOW = 4,
    WS2812B_PALETTE_PURPLE = 5,
    WS2812B_PALETTE_WHITE = 6,
    WS2812B_PALETTE_CYAN = 7,
    WS2812B_PALETTE_ORANGE = 8
} WS2812B_PaletteId;

typedef enum
{
    WS2812B_TEMPLATE_CLEAR = 0,
    WS2812B_TEMPLATE_FULL,
    WS2812B_TEMPLATE_CHECKERBOARD,
    WS2812B_TEMPLATE_BORDER,
    WS2812B_TEMPLATE_CROSS,
    WS2812B_TEMPLATE_DIAGONAL
} WS2812B_TemplateId;

void WS2812B_Init(void);
void WS2812B_SetBrightness(uint8_t percent);
uint8_t WS2812B_GetBrightness(void);
void WS2812B_Clear(void);
void WS2812B_SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void WS2812B_FillRgb(uint8_t r, uint8_t g, uint8_t b);
void WS2812B_FillColor(WS2812B_Color color);
void WS2812B_ApplyBitmapRows(const uint8_t *rows,
                             uint8_t row_count,
                             WS2812B_Color on_color,
                             WS2812B_Color off_color);
void WS2812B_ApplyBitmapRows16(const uint8_t rows[MOLE_RGB16_ROWS][2],
                               WS2812B_Color on_color,
                               WS2812B_Color off_color);
void WS2812B_ApplyRgbBuffer16(const uint8_t *rgb,
                              uint16_t rgb_len);
uint8_t WS2812B_ShowBitmapRows(const uint8_t *rows,
                               uint8_t row_count,
                               WS2812B_Color on_color,
                               WS2812B_Color off_color,
                               uint8_t use_polling);
uint8_t WS2812B_ShowBitmapRows16(const uint8_t rows[MOLE_RGB16_ROWS][2],
                                 WS2812B_Color on_color,
                                 WS2812B_Color off_color,
                                 uint8_t use_polling);
uint8_t WS2812B_ShowRgbBuffer16(const uint8_t *rgb,
                                uint16_t rgb_len,
                                uint8_t use_polling);
uint8_t WS2812B_ShowTemplate(WS2812B_TemplateId template_id,
                             WS2812B_Color on_color,
                             WS2812B_Color off_color,
                             uint8_t use_polling);
uint8_t WS2812B_Refresh(void);
uint8_t WS2812B_RefreshPolling(void);
void WS2812B_DiagnosticGpioProbe(uint8_t pulses, uint32_t half_period_ms);
uint8_t WS2812B_ShowMoleFrame(const MoleLedFrame *frame);
uint8_t WS2812B_ShowMoleFrame16Mono(const MoleLedFrame16Mono *frame);
WS2812B_Color WS2812B_ColorMake(uint8_t r, uint8_t g, uint8_t b);
WS2812B_Color WS2812B_ColorFromPalette(WS2812B_PaletteId palette_id);
WS2812B_Color WS2812B_ColorFromProtocol(uint8_t color_code);

#endif /* WS2812B_H */
