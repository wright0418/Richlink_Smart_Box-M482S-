#ifndef WS2812B_DIGITS_H
#define WS2812B_DIGITS_H

#include <stdint.h>
#include "ws2812b.h"
#include "../app/algorithms/squat_detect.h"

void WS2812B_DrawDigit3x5(uint8_t digit, uint8_t x, uint8_t y, WS2812B_Color color);
void WS2812B_ShowNumber2Digit(uint16_t value, WS2812B_Color color);
void WS2812B_ShowProgressBar8(uint8_t level, WS2812B_Color color_on, WS2812B_Color color_off);
void WS2812B_ShowSquatScreen(uint16_t count, SquatPhase phase, uint8_t progress);
void WS2812B_ShowSquatFlash(WS2812B_Color color, uint32_t hold_ms);

#endif /* WS2812B_DIGITS_H */
