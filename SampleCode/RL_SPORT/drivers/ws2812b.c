#include "ws2812b.h"
#include "ws2812b_templates.h"

#include "NuMicro.h"
#include "../board/gpio.h"
#include "../drivers/timer.h"
#include "../project_config.h"

#include <stddef.h>
#include <string.h>

#ifndef WS2812B_SPI_CLOCK_HZ
#define WS2812B_SPI_CLOCK_HZ 6000000u
#endif

#ifndef WS2812B_PDMA_CH
#define WS2812B_PDMA_CH 0u
#endif

#define WS2812B_SPI_ZERO_CODE 0xC0u
#define WS2812B_SPI_ONE_CODE 0xF8u
#define WS2812B_BYTES_PER_PIXEL 24u
#define WS2812B_LATCH_BYTES 48u
#ifndef MOLE_WS2812_LED_COUNT
#define MOLE_WS2812_LED_COUNT MOLE_LED_COUNT
#endif
#define WS2812B_SPI_BUF_SIZE ((MOLE_WS2812_LED_COUNT * WS2812B_BYTES_PER_PIXEL) + WS2812B_LATCH_BYTES)
#define WS2812B_PDMA_TIMEOUT_LOOPS 2000000u
#define WS2812B_POLL_TIMEOUT_LOOPS 2000000u
#define WS2812B_FRAME_FLUSH_DELAY_MS 4u
#define WS2812B_BITMAP_COLS 8u

typedef struct
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
} WS2812B_GrbPixel;

static WS2812B_GrbPixel s_pixels[MOLE_WS2812_LED_COUNT];
static uint8_t s_spi_buf[WS2812B_SPI_BUF_SIZE] __attribute__((aligned(4)));
static uint8_t s_brightness_percent = MOLE_LED_DEFAULT_BRIGHTNESS_PERCENT;
static uint8_t s_initialized = 0u;
static uint8_t s_refresh_debug_printed = 0u;

static void WS2812B_RecoverSpiState(void);

static uint8_t WS2812B_Scale(uint8_t value)
{
    uint16_t scaled = (uint16_t)(((uint16_t)value * (uint16_t)s_brightness_percent + 50u) / 100u);
    return (scaled > 255u) ? 255u : (uint8_t)scaled;
}

static void WS2812B_EncodeByte(uint8_t value, uint8_t *out)
{
    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
        uint8_t mask = (uint8_t)(0x80u >> bit);
        out[bit] = (value & mask) ? WS2812B_SPI_ONE_CODE : WS2812B_SPI_ZERO_CODE;
    }
}

static void WS2812B_BuildSpiBuffer(void)
{
    uint32_t offset = 0u;

    for (uint16_t i = 0u; i < MOLE_WS2812_LED_COUNT; i++)
    {
        WS2812B_EncodeByte(WS2812B_Scale(s_pixels[i].g), &s_spi_buf[offset]);
        offset += 8u;
        WS2812B_EncodeByte(WS2812B_Scale(s_pixels[i].r), &s_spi_buf[offset]);
        offset += 8u;
        WS2812B_EncodeByte(WS2812B_Scale(s_pixels[i].b), &s_spi_buf[offset]);
        offset += 8u;
    }

    memset(&s_spi_buf[offset], 0, WS2812B_LATCH_BYTES);
}

static void WS2812B_DrainRxFifo(void)
{
    while (SPI_GET_RX_FIFO_EMPTY_FLAG(SPI0) == 0u)
    {
        (void)SPI_READ_RX(SPI0);
    }
}

static void WS2812B_RecoverSpiState(void)
{
    SPI_DISABLE_TX_PDMA(SPI0);
    SPI_Close(SPI0);
    SYS_ResetModule(SPI0_RST);

    SPI_Open(SPI0, SPI_MASTER, SPI_MODE_0, 8u, WS2812B_SPI_CLOCK_HZ);
    SPI_DISABLE_BYTE_REORDER(SPI0);
    SPI_DisableAutoSS(SPI0);
    SPI_SetFIFO(SPI0, 4u, 4u);
    SPI_ClearRxFIFO(SPI0);
    SPI_ClearTxFIFO(SPI0);
}

WS2812B_Color WS2812B_ColorMake(uint8_t r, uint8_t g, uint8_t b)
{
    WS2812B_Color color;
    color.r = r;
    color.g = g;
    color.b = b;
    return color;
}

WS2812B_Color WS2812B_ColorFromPalette(WS2812B_PaletteId palette_id)
{
    switch (palette_id)
    {
    case WS2812B_PALETTE_OFF:
        return WS2812B_ColorMake(0u, 0u, 0u);
    case WS2812B_PALETTE_RED:
        return WS2812B_ColorMake(255u, 0u, 0u);
    case WS2812B_PALETTE_GREEN:
        return WS2812B_ColorMake(0u, 255u, 0u);
    case WS2812B_PALETTE_BLUE:
        return WS2812B_ColorMake(0u, 0u, 255u);
    case WS2812B_PALETTE_YELLOW:
        return WS2812B_ColorMake(255u, 255u, 0u);
    case WS2812B_PALETTE_PURPLE:
        return WS2812B_ColorMake(128u, 0u, 128u);
    case WS2812B_PALETTE_CYAN:
        return WS2812B_ColorMake(0u, 255u, 255u);
    case WS2812B_PALETTE_ORANGE:
        return WS2812B_ColorMake(255u, 128u, 0u);
    case WS2812B_PALETTE_WHITE:
    default:
        return WS2812B_ColorMake(255u, 255u, 255u);
    }
}

static uint8_t WS2812B_RefreshViaPdma(void)
{
    uint32_t timeout = WS2812B_PDMA_TIMEOUT_LOOPS;
    uint32_t int_status;

    SPI_DISABLE_TX_PDMA(SPI0);
    SPI_ClearRxFIFO(SPI0);
    SPI_ClearTxFIFO(SPI0);
    PDMA_CLR_TD_FLAG(PDMA, (uint32_t)(1u << WS2812B_PDMA_CH));
    PDMA_CLR_ABORT_FLAG(PDMA, (uint32_t)(1u << WS2812B_PDMA_CH));

    PDMA_SetTransferCnt(PDMA, WS2812B_PDMA_CH, PDMA_WIDTH_8, WS2812B_SPI_BUF_SIZE);
    PDMA_SetTransferAddr(PDMA,
                         WS2812B_PDMA_CH,
                         (uint32_t)s_spi_buf,
                         PDMA_SAR_INC,
                         (uint32_t)&SPI0->TX,
                         PDMA_DAR_FIX);
    PDMA_SetTransferMode(PDMA, WS2812B_PDMA_CH, PDMA_SPI0_TX, FALSE, 0u);
    PDMA_SetBurstType(PDMA, WS2812B_PDMA_CH, PDMA_REQ_SINGLE, 0u);
    PDMA->DSCT[WS2812B_PDMA_CH].CTL |= PDMA_DSCT_CTL_TBINTDIS_Msk;

    SPI_TRIGGER_TX_PDMA(SPI0);

    while (timeout-- > 0u)
    {
        WS2812B_DrainRxFifo();
        int_status = PDMA_GET_INT_STATUS(PDMA);

        if ((int_status & PDMA_INTSTS_TDIF_Msk) != 0u)
        {
            if ((PDMA_GET_TD_STS(PDMA) & (uint32_t)(1u << WS2812B_PDMA_CH)) != 0u)
            {
                PDMA_CLR_TD_FLAG(PDMA, (uint32_t)(1u << WS2812B_PDMA_CH));
                SPI_DISABLE_TX_PDMA(SPI0);
                delay_ms(WS2812B_FRAME_FLUSH_DELAY_MS);
                return 1u;
            }
        }

        if ((int_status & PDMA_INTSTS_ABTIF_Msk) != 0u)
        {
            uint32_t abort_status = PDMA_GET_ABORT_STS(PDMA);
            PDMA_CLR_ABORT_FLAG(PDMA, abort_status);
            SPI_DISABLE_TX_PDMA(SPI0);
            return 0u;
        }

        if ((int_status & (PDMA_INTSTS_REQTOF0_Msk | PDMA_INTSTS_REQTOF1_Msk)) != 0u)
        {
            PDMA->INTSTS = int_status & (PDMA_INTSTS_REQTOF0_Msk | PDMA_INTSTS_REQTOF1_Msk);
            SPI_DISABLE_TX_PDMA(SPI0);
            return 0u;
        }
    }

    SPI_DISABLE_TX_PDMA(SPI0);
    return 0u;
}

WS2812B_Color WS2812B_ColorFromProtocol(uint8_t color_code)
{
    return WS2812B_ColorFromPalette((WS2812B_PaletteId)color_code);
}

void WS2812B_Init(void)
{
    if (s_initialized != 0u)
    {
        DBG_PRINT("[WS2812] Init skipped (already initialized)\n");
        return;
    }

    DBG_PRINT("[WS2812] Init start clk=%luHz leds=%u\n",
              (unsigned long)WS2812B_SPI_CLOCK_HZ,
              (unsigned)MOLE_WS2812_LED_COUNT);

    CLK_EnableModuleClock(PDMA_MODULE);
    CLK_EnableModuleClock(SPI0_MODULE);
    CLK_SetModuleClock(SPI0_MODULE, CLK_CLKSEL2_SPI0SEL_PCLK1, MODULE_NoMsk);
    Board_ConfigWs2812SpiPin();

    WS2812B_RecoverSpiState();

    PDMA_Open(PDMA, (uint32_t)(1u << WS2812B_PDMA_CH));
    SPI_DISABLE_TX_PDMA(SPI0);

    s_initialized = 1u;
    WS2812B_Clear();
    DBG_PRINT("[WS2812] Init done, sending clear frame\n");
    if (WS2812B_Refresh() != 0u)
    {
        DBG_PRINT("[WS2812] Clear frame OK\n");
    }
    else
    {
        DBG_PRINT("[WS2812] Clear frame FAIL\n");
    }
}

void WS2812B_SetBrightness(uint8_t percent)
{
    if (percent > 100u)
    {
        percent = 100u;
    }
    s_brightness_percent = percent;
}

uint8_t WS2812B_GetBrightness(void)
{
    return s_brightness_percent;
}

void WS2812B_Clear(void)
{
    memset(s_pixels, 0, sizeof(s_pixels));
}

void WS2812B_SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= MOLE_WS2812_LED_COUNT)
    {
        return;
    }

    s_pixels[index].r = r;
    s_pixels[index].g = g;
    s_pixels[index].b = b;
}

void WS2812B_FillRgb(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t i = 0u; i < MOLE_WS2812_LED_COUNT; i++)
    {
        WS2812B_SetPixel(i, r, g, b);
    }
}

void WS2812B_FillColor(WS2812B_Color color)
{
    WS2812B_FillRgb(color.r, color.g, color.b);
}

void WS2812B_ApplyBitmapRows(const uint8_t *rows,
                             uint8_t row_count,
                             WS2812B_Color on_color,
                             WS2812B_Color off_color)
{
    uint8_t limited_rows = row_count;

    if (limited_rows > MOLE_LED_ROWS)
    {
        limited_rows = MOLE_LED_ROWS;
    }

    if (rows == NULL)
    {
        WS2812B_FillColor(off_color);
        return;
    }

    WS2812B_FillColor(off_color);

    for (uint8_t row = 0u; row < MOLE_LED_ROWS; row++)
    {
        uint8_t row_bits = (row < limited_rows) ? rows[row] : 0u;
        for (uint8_t col = 0u; col < WS2812B_BITMAP_COLS; col++)
        {
            uint16_t index = (uint16_t)((uint16_t)row * WS2812B_BITMAP_COLS + col);
            uint8_t active = (uint8_t)((row_bits & (uint8_t)(0x80u >> col)) != 0u);
            WS2812B_Color color = active ? on_color : off_color;
            WS2812B_SetPixel(index, color.r, color.g, color.b);
        }
    }
}

void WS2812B_ApplyBitmapRows16(const uint8_t rows[MOLE_RGB16_ROWS][2],
                               WS2812B_Color on_color,
                               WS2812B_Color off_color)
{
#if MOLE_ENABLE_RGB16X16
    WS2812B_FillColor(off_color);

    if (rows == NULL)
    {
        return;
    }

    for (uint8_t row = 0u; row < MOLE_RGB16_ROWS; row++)
    {
        uint16_t row_bits = (uint16_t)(((uint16_t)rows[row][0] << 8u) | rows[row][1]);
        for (uint8_t col = 0u; col < MOLE_RGB16_COLS; col++)
        {
            uint16_t index = (uint16_t)((uint16_t)row * MOLE_RGB16_COLS + col);
            uint8_t active = (row_bits & (uint16_t)(0x8000u >> col)) ? 1u : 0u;
            WS2812B_Color color = active ? on_color : off_color;
            WS2812B_SetPixel(index, color.r, color.g, color.b);
        }
    }
#else
    (void)rows;
    (void)on_color;
    (void)off_color;
#endif
}

void WS2812B_ApplyRgbBuffer16(const uint8_t *rgb, uint16_t rgb_len)
{
#if MOLE_ENABLE_RGB16X16 && MOLE_ENABLE_RGB16X16_COLOR
    uint16_t pixel_count;

    WS2812B_Clear();

    if (rgb == NULL)
    {
        return;
    }

    if (rgb_len > MOLE_RGB16_COLOR_BYTES)
    {
        rgb_len = MOLE_RGB16_COLOR_BYTES;
    }

    pixel_count = (uint16_t)(rgb_len / 3u);
    for (uint16_t i = 0u; i < pixel_count; i++)
    {
        uint16_t offset = (uint16_t)(i * 3u);
        WS2812B_SetPixel(i, rgb[offset], rgb[offset + 1u], rgb[offset + 2u]);
    }
#else
    (void)rgb;
    (void)rgb_len;
#endif
}

uint8_t WS2812B_ShowBitmapRows(const uint8_t *rows,
                               uint8_t row_count,
                               WS2812B_Color on_color,
                               WS2812B_Color off_color,
                               uint8_t use_polling)
{
    WS2812B_ApplyBitmapRows(rows, row_count, on_color, off_color);
    return (use_polling != 0u) ? WS2812B_RefreshPolling() : WS2812B_Refresh();
}

uint8_t WS2812B_ShowBitmapRows16(const uint8_t rows[MOLE_RGB16_ROWS][2],
                                 WS2812B_Color on_color,
                                 WS2812B_Color off_color,
                                 uint8_t use_polling)
{
    WS2812B_ApplyBitmapRows16(rows, on_color, off_color);
    return (use_polling != 0u) ? WS2812B_RefreshPolling() : WS2812B_Refresh();
}

uint8_t WS2812B_ShowRgbBuffer16(const uint8_t *rgb, uint16_t rgb_len, uint8_t use_polling)
{
    WS2812B_ApplyRgbBuffer16(rgb, rgb_len);
    return (use_polling != 0u) ? WS2812B_RefreshPolling() : WS2812B_Refresh();
}

uint8_t WS2812B_ShowTemplate(WS2812B_TemplateId template_id,
                             WS2812B_Color on_color,
                             WS2812B_Color off_color,
                             uint8_t use_polling)
{
    uint8_t row_count = 0u;
    const uint8_t *rows = WS2812B_GetTemplateRows(template_id, &row_count);
    return WS2812B_ShowBitmapRows(rows, row_count, on_color, off_color, use_polling);
}

uint8_t WS2812B_Refresh(void)
{
    uint8_t result;

    if (s_initialized == 0u)
    {
        DBG_PRINT("[WS2812] Refresh requested before init\n");
        return 0u;
    }

    Board_ConfigWs2812SpiPin();
    WS2812B_RecoverSpiState();
    WS2812B_BuildSpiBuffer();

    if (s_refresh_debug_printed == 0u)
    {
        DBG_PRINT("[WS2812] Refresh try PDMA first\n");
    }

    result = WS2812B_RefreshViaPdma();
    if (result != 0u)
    {
        if (s_refresh_debug_printed == 0u)
        {
            DBG_PRINT("[WS2812] Refresh PDMA OK\n");
            s_refresh_debug_printed = 1u;
        }
        return 1u;
    }

    DBG_PRINT("[WS2812] Refresh PDMA FAIL -> fallback polling\n");
    result = WS2812B_RefreshPolling();
    DBG_PRINT("[WS2812] Refresh polling %s\n", result ? "OK" : "FAIL");
    s_refresh_debug_printed = 1u;
    return result;
}

uint8_t WS2812B_RefreshPolling(void)
{
    uint32_t timeout;
    uint8_t result;

    if (s_initialized == 0u)
    {
        return 0u;
    }

    Board_ConfigWs2812SpiPin();
    WS2812B_RecoverSpiState();
    WS2812B_BuildSpiBuffer();

    SPI_DISABLE_TX_PDMA(SPI0);
    SPI_ClearRxFIFO(SPI0);
    SPI_ClearTxFIFO(SPI0);

    DBG_PRINT("[WS2812] Polling refresh start bytes=%lu\n", (unsigned long)WS2812B_SPI_BUF_SIZE);

    for (uint32_t i = 0u; i < WS2812B_SPI_BUF_SIZE; i++)
    {
        timeout = WS2812B_POLL_TIMEOUT_LOOPS;
        while (SPI_GET_TX_FIFO_FULL_FLAG(SPI0) != 0u)
        {
            WS2812B_DrainRxFifo();
            if (timeout-- == 0u)
            {
                DBG_PRINT("[WS2812] Polling timeout waiting TX room i=%lu status=0x%08lX\n",
                          (unsigned long)i,
                          (unsigned long)SPI0->STATUS);
                return 0u;
            }
        }

        SPI_WRITE_TX(SPI0, s_spi_buf[i]);
        WS2812B_DrainRxFifo();
    }

    DBG_PRINT("[WS2812] Polling queued all bytes\n");

    timeout = WS2812B_POLL_TIMEOUT_LOOPS;
    while (SPI_GET_TX_FIFO_EMPTY_FLAG(SPI0) == 0u)
    {
        WS2812B_DrainRxFifo();
        if (timeout-- == 0u)
        {
            DBG_PRINT("[WS2812] Polling timeout waiting TX empty status=0x%08lX\n",
                      (unsigned long)SPI0->STATUS);
            return 0u;
        }
    }

    DBG_PRINT("[WS2812] Polling TX FIFO empty\n");
    delay_ms(WS2812B_FRAME_FLUSH_DELAY_MS);
    result = 1u;
    DBG_PRINT("[WS2812] Polling flush delay done\n");
    return result;
}

void WS2812B_DiagnosticGpioProbe(uint8_t pulses, uint32_t half_period_ms)
{
    SPI_DISABLE_TX_PDMA(SPI0);
    Board_SetWs2812DataPinSafe();

    for (uint8_t i = 0u; i < pulses; i++)
    {
        PF->DOUT |= BIT6;
        delay_ms(half_period_ms);
        PF->DOUT &= ~BIT6;
        delay_ms(half_period_ms);
    }

    Board_ConfigWs2812SpiPin();
}

uint8_t WS2812B_ShowMoleFrame(const MoleLedFrame *frame)
{
    if (frame == NULL)
    {
        return 0u;
    }

    return WS2812B_ShowBitmapRows(frame->rows,
                                  MOLE_LED_ROWS,
                                  WS2812B_ColorFromProtocol(frame->color),
                                  WS2812B_ColorFromPalette(WS2812B_PALETTE_OFF),
                                  0u);
}

uint8_t WS2812B_ShowMoleFrame16Mono(const MoleLedFrame16Mono *frame)
{
#if MOLE_ENABLE_RGB16X16
    if (frame == NULL)
    {
        return 0u;
    }

    return WS2812B_ShowBitmapRows16(frame->rows,
                                    WS2812B_ColorFromProtocol(frame->color),
                                    WS2812B_ColorFromPalette(WS2812B_PALETTE_OFF),
                                    0u);
#else
    (void)frame;
    return 0u;
#endif
}
