/**
 * @file app/mole_game.c
 * @brief Mole game BLE protocol and WS2812 frame handling.
 *
 * This module implements the BLE-driven Mole game profile, including packet
 * parsing, LED frame staging, RGB16 chunked transfer, and hit detection.
 */
#include "mole_game.h"

#include "../ble.h"
#include "../drivers/gsensor.h"
#include "../drivers/ws2812b.h"
#include "../drivers/ws2812b_patterns.h"
#include "../drivers/timer.h"
#include "../project_config.h"
#include "../protocol/mole_packet.h"
#include "algorithms/mole_hit_detect.h"

#include <string.h>

#define MOLE_RAW_READ_CHUNK 64u
#define MOLE_HIT_CODE_KEYA 0x01u
#define MOLE_HIT_CODE_KEYB 0x02u

#if MOLE_TEST_TRACE_ENABLE
extern int printf(const char *format, ...);
#define MOLE_TRACE_PRINT(fmt, ...) printf("[MOLE_TEST] " fmt, ##__VA_ARGS__)
#else
#define MOLE_TRACE_PRINT(fmt, ...)
#endif

/* Game state context */
typedef struct
{
    uint8_t brightness_percent;   /* Current brightness (0-100) */
    uint8_t hit_detection_method; /* Bitmask: BUTTON | GSENSOR (default: BUTTON only) */
} MoleGameContext;

static MolePacketParser s_packet_parser;
static MoleHitDetector s_hit_detector;
static MoleLedFrame s_last_frame;
static MoleLedFrame s_pending_frame;
#if MOLE_ENABLE_RGB16X16 && MOLE_ENABLE_RGB16X16_COLOR
static uint8_t s_rgb16_staging[MOLE_RGB16_COLOR_BYTES];
static uint16_t s_rgb16_bytes_received = 0u;
static uint8_t s_rgb16_frame_active = 0u;
static uint8_t s_rgb16_frame_id = 0u;
static uint8_t s_rgb16_next_chunk_index = 0u;
#endif
static uint32_t s_last_gsensor_sample_ms = 0u;
static uint32_t s_last_led_apply_ms = 0u;
static uint8_t s_has_frame = 0u;
static uint8_t s_has_display = 0u;
static uint8_t s_has_pending_frame = 0u;
static MoleGameContext s_game_ctx = {
    .brightness_percent = MOLE_LED_DEFAULT_BRIGHTNESS_PERCENT,
    .hit_detection_method = MOLE_HIT_METHOD_BUTTON /* Default: button only */
};

static const char *MoleGame_PacketTypeToString(MolePacketType type)
{
    switch (type)
    {
    case MOLE_PACKET_LED:
        return "LED";
    case MOLE_PACKET_LED16_MONO:
        return "LED16_MONO";
    case MOLE_PACKET_RGB16_CHUNK:
        return "RGB16_CHUNK";
    case MOLE_PACKET_BRIGHTNESS:
        return "BRIGHTNESS";
    case MOLE_PACKET_BRIGHTNESS_CMD:
        return "BRIGHTNESS_CMD";
    case MOLE_PACKET_HIT_CONFIG:
        return "HIT_CONFIG";
    case MOLE_PACKET_NONE:
    default:
        return "NONE";
    }
}

static const char *MoleGame_PacketVariantToString(MolePacketVariant variant)
{
    return (variant == MOLE_PACKET_VARIANT_APP) ? "APP" : "EPY";
}

static void MoleGame_ApplyLedFrame(const MoleLedFrame *frame, const char *reason)
{
    uint8_t show_ok;
    uint32_t now = get_ticks_ms();

    if (frame == NULL)
    {
        return;
    }

    s_last_frame = *frame;
    s_has_frame = 1u;
    s_has_display = 1u;
    s_last_led_apply_ms = now;

    MOLE_TRACE_PRINT("RX LED color=%u target=%u rows=%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\r\n",
                     (unsigned)frame->color,
                     (unsigned)frame->target_tag,
                     (unsigned)frame->rows[0],
                     (unsigned)frame->rows[1],
                     (unsigned)frame->rows[2],
                     (unsigned)frame->rows[3],
                     (unsigned)frame->rows[4],
                     (unsigned)frame->rows[5],
                     (unsigned)frame->rows[6],
                     (unsigned)frame->rows[7]);
    MOLE_TRACE_PRINT("DISPLAY STEP1: apply LED frame -> WS2812B_ShowMoleFrame (%s)\r\n",
                     (reason != NULL) ? reason : "direct");
    show_ok = WS2812B_ShowMoleFrame(&s_last_frame);
    MOLE_TRACE_PRINT("DISPLAY STEP2: WS2812B_ShowMoleFrame result=%s\r\n", show_ok ? "OK" : "FAIL");
}

#if MOLE_ENABLE_RGB16X16
static void MoleGame_ApplyLed16MonoFrame(const MoleLedFrame16Mono *frame, const char *reason)
{
    uint8_t show_ok;
    uint32_t now = get_ticks_ms();

    if (frame == NULL)
    {
        return;
    }

    s_has_display = 1u;
    s_last_led_apply_ms = now;

    MOLE_TRACE_PRINT("RX LED16_MONO color=%u target=%u row0=%02X%02X row15=%02X%02X\r\n",
                     (unsigned)frame->color,
                     (unsigned)frame->target_tag,
                     (unsigned)frame->rows[0][0],
                     (unsigned)frame->rows[0][1],
                     (unsigned)frame->rows[15][0],
                     (unsigned)frame->rows[15][1]);
    MOLE_TRACE_PRINT("DISPLAY STEP1: apply LED16 mono frame -> WS2812B_ShowMoleFrame16Mono (%s)\r\n",
                     (reason != NULL) ? reason : "direct");
    show_ok = WS2812B_ShowMoleFrame16Mono(frame);
    MOLE_TRACE_PRINT("DISPLAY STEP2: WS2812B_ShowMoleFrame16Mono result=%s\r\n", show_ok ? "OK" : "FAIL");
}
#endif

#if MOLE_ENABLE_RGB16X16 && MOLE_ENABLE_RGB16X16_COLOR
static void MoleGame_ResetRgb16Staging(void)
{
    memset(s_rgb16_staging, 0, sizeof(s_rgb16_staging));
    s_rgb16_bytes_received = 0u;
    s_rgb16_frame_active = 0u;
    s_rgb16_frame_id = 0u;
    s_rgb16_next_chunk_index = 0u;
}

static void MoleGame_HandleRgb16Chunk(const MoleRgb16Chunk *chunk)
{
    if (chunk == NULL)
    {
        return;
    }

    switch (chunk->op)
    {
    case MOLE_RGB16_CHUNK_OP_START:
        MoleGame_ResetRgb16Staging();
        s_rgb16_frame_active = 1u;
        s_rgb16_frame_id = chunk->frame_id;
        MOLE_TRACE_PRINT("RGB16 START frame=%u\r\n", (unsigned)s_rgb16_frame_id);
        break;

    case MOLE_RGB16_CHUNK_OP_DATA:
        if ((s_rgb16_frame_active == 0u) ||
            (chunk->frame_id != s_rgb16_frame_id) ||
            (chunk->chunk_index != s_rgb16_next_chunk_index) ||
            (chunk->offset != s_rgb16_bytes_received) ||
            (((uint32_t)chunk->offset + chunk->payload_len) > MOLE_RGB16_COLOR_BYTES))
        {
            MOLE_TRACE_PRINT("RGB16 DATA drop frame=%u chunk=%u offset=%u len=%u active=%u expected_frame=%u expected_chunk=%u expected_offset=%u\r\n",
                             (unsigned)chunk->frame_id,
                             (unsigned)chunk->chunk_index,
                             (unsigned)chunk->offset,
                             (unsigned)chunk->payload_len,
                             (unsigned)s_rgb16_frame_active,
                             (unsigned)s_rgb16_frame_id,
                             (unsigned)s_rgb16_next_chunk_index,
                             (unsigned)s_rgb16_bytes_received);
            MoleGame_ResetRgb16Staging();
            break;
        }

        memcpy(&s_rgb16_staging[chunk->offset], chunk->payload, chunk->payload_len);
        s_rgb16_bytes_received = (uint16_t)(s_rgb16_bytes_received + chunk->payload_len);
        s_rgb16_next_chunk_index++;
        MOLE_TRACE_PRINT("RGB16 DATA frame=%u chunk=%u bytes=%u/%u\r\n",
                         (unsigned)chunk->frame_id,
                         (unsigned)chunk->chunk_index,
                         (unsigned)s_rgb16_bytes_received,
                         (unsigned)MOLE_RGB16_COLOR_BYTES);
        break;

    case MOLE_RGB16_CHUNK_OP_COMMIT:
        if ((s_rgb16_frame_active == 0u) ||
            (chunk->frame_id != s_rgb16_frame_id) ||
            (s_rgb16_bytes_received != MOLE_RGB16_COLOR_BYTES))
        {
            MOLE_TRACE_PRINT("RGB16 COMMIT drop frame=%u active=%u expected_frame=%u bytes=%u/%u\r\n",
                             (unsigned)chunk->frame_id,
                             (unsigned)s_rgb16_frame_active,
                             (unsigned)s_rgb16_frame_id,
                             (unsigned)s_rgb16_bytes_received,
                             (unsigned)MOLE_RGB16_COLOR_BYTES);
            MoleGame_ResetRgb16Staging();
            break;
        }

        MOLE_TRACE_PRINT("RGB16 COMMIT apply frame=%u bytes=%u\r\n",
                         (unsigned)chunk->frame_id,
                         (unsigned)s_rgb16_bytes_received);
        s_has_display = 1u;
        s_last_led_apply_ms = get_ticks_ms();
        (void)WS2812B_ShowRgbBuffer16(s_rgb16_staging, MOLE_RGB16_COLOR_BYTES, 0u);
        MoleGame_ResetRgb16Staging();
        break;

    case MOLE_RGB16_CHUNK_OP_CANCEL:
    default:
        MOLE_TRACE_PRINT("RGB16 CANCEL frame=%u\r\n", (unsigned)chunk->frame_id);
        MoleGame_ResetRgb16Staging();
        break;
    }
}
#endif

static void MoleGame_CommitPendingLedFrameIfDue(void)
{
    if (!s_has_pending_frame)
    {
        return;
    }

    if (s_has_frame && (get_elapsed_ms(s_last_led_apply_ms) < MOLE_LED_MIN_HOLD_MS))
    {
        return;
    }

    s_has_pending_frame = 0u;
    MoleGame_ApplyLedFrame(&s_pending_frame, "pending-release");
}

static void MoleGame_FillAllPixels(uint8_t r, uint8_t g, uint8_t b)
{
    WS2812B_FillRgb(r, g, b);
}

static void MoleGame_FillRainbowBy16Leds(void)
{
    static const WS2812B_Color k_rainbow[] = {
        {255u, 0u, 0u},
        {255u, 64u, 0u},
        {255u, 128u, 0u},
        {255u, 255u, 0u},
        {0u, 255u, 0u},
        {0u, 255u, 128u},
        {0u, 255u, 255u},
        {0u, 128u, 255u},
        {0u, 0u, 255u},
        {64u, 0u, 255u},
        {128u, 0u, 255u},
        {255u, 0u, 255u},
        {255u, 0u, 128u},
        {255u, 0u, 64u},
        {255u, 32u, 32u},
        {255u, 255u, 255u}};
    const uint8_t color_count = (uint8_t)(sizeof(k_rainbow) / sizeof(k_rainbow[0]));

    for (uint16_t i = 0u; i < (uint16_t)MOLE_WS2812_LED_COUNT; i++)
    {
        uint8_t color_idx = (uint8_t)((i / 16u) % color_count);
        WS2812B_Color color = k_rainbow[color_idx];
        WS2812B_SetPixel(i, color.r, color.g, color.b);
    }
}

static void MoleGame_LogBootSixAxisSamples(void)
{
    DBG_PRINT("[MOLE] Boot 6-axis precheck start samples=%u sensor=%s addr=0x%02X\n",
              (unsigned)MOLE_WS2812_BOOT_SELF_TEST_SENSOR_SAMPLES,
              GsensorGetDeviceName(),
              (unsigned)GsensorGetI2CAddress());

    for (uint32_t i = 0u; i < MOLE_WS2812_BOOT_SELF_TEST_SENSOR_SAMPLES; i++)
    {
        int16_t acc[3] = {0};
        int16_t gyro[3] = {0};
        uint8_t ok = GsensorReadSixAxis(acc, gyro);

        DBG_PRINT("[MOLE] 6AXIS[%lu] %s ACC=%d,%d,%d GYR=%d,%d,%d\n",
                  (unsigned long)(i + 1u),
                  ok ? "OK" : "FAIL",
                  acc[0], acc[1], acc[2],
                  gyro[0], gyro[1], gyro[2]);
        delay_ms(20u);
    }
}

static void MoleGame_RunWs2812BootSelfTest(void)
{
#if MOLE_WS2812_BOOT_SELF_TEST
    const uint8_t saved_brightness = WS2812B_GetBrightness();
    uint8_t refresh_ok;

    DBG_PRINT("[MOLE] WS2812 boot self-test start (saved brightness=%u)\n", (unsigned)saved_brightness);

    WS2812B_SetBrightness(MOLE_WS2812_BOOT_SELF_TEST_BRIGHTNESS_PERCENT);
    DBG_PRINT("[MOLE] WS2812 brightness forced to %u%%\n",
              (unsigned)MOLE_WS2812_BOOT_SELF_TEST_BRIGHTNESS_PERCENT);

#if MOLE_WS2812_DIAG_REPEAT
    while (1)
    {
        WS2812B_DiagnosticGpioProbe(3u, MOLE_WS2812_DIAG_GPIO_PROBE_MS);

        MoleGame_FillAllPixels(255u, 0u, 0u); /* Red: SPI polling path (no PDMA). */
        (void)WS2812B_RefreshPolling();
        delay_ms(MOLE_WS2812_DIAG_STEP_MS);

        MoleGame_FillAllPixels(0u, 255u, 0u); /* Green: SPI + PDMA path. */
        (void)WS2812B_Refresh();
        delay_ms(MOLE_WS2812_DIAG_STEP_MS);

        MoleGame_FillAllPixels(0u, 0u, 255u); /* Blue: SPI polling path (repeat check). */
        (void)WS2812B_RefreshPolling();
        delay_ms(MOLE_WS2812_DIAG_STEP_MS);

        MoleGame_FillAllPixels(255u, 255u, 255u); /* White: SPI + PDMA path (all channels). */
        (void)WS2812B_Refresh();
        delay_ms(MOLE_WS2812_DIAG_STEP_MS);

        WS2812B_Clear();
        (void)WS2812B_RefreshPolling();
        delay_ms(MOLE_WS2812_DIAG_STEP_MS);
    }
#else
    const uint32_t hold_ms = MOLE_WS2812_BOOT_SELF_TEST_COLOR_HOLD_MS;

    DBG_PRINT("[MOLE] Self-test 16x16 sequence start (R/G/B=%lums, rainbow=16-led stripe)\n",
              (unsigned long)hold_ms);

    MoleGame_FillAllPixels(255u, 0u, 0u);
    refresh_ok = WS2812B_Refresh();
    DBG_PRINT("[MOLE] Self-test RED refresh=%u\n", (unsigned)refresh_ok);
    delay_ms(hold_ms);

    MoleGame_FillAllPixels(0u, 255u, 0u);
    refresh_ok = WS2812B_Refresh();
    DBG_PRINT("[MOLE] Self-test GREEN refresh=%u\n", (unsigned)refresh_ok);
    delay_ms(hold_ms);

    MoleGame_FillAllPixels(0u, 0u, 255u);
    refresh_ok = WS2812B_Refresh();
    DBG_PRINT("[MOLE] Self-test BLUE refresh=%u\n", (unsigned)refresh_ok);
    delay_ms(hold_ms);

    MoleGame_FillRainbowBy16Leds();
    refresh_ok = WS2812B_Refresh();
    DBG_PRINT("[MOLE] Self-test RAINBOW refresh=%u\n", (unsigned)refresh_ok);

    WS2812B_SetBrightness(saved_brightness);
    DBG_PRINT("[MOLE] WS2812 boot self-test done, brightness restored=%u\n", (unsigned)saved_brightness);
#endif
#endif
}

static void MoleGame_SendHitReport(uint8_t hit_report)
{
    if (hit_report == MOLE_HIT_CODE_KEYA)
    {
        MOLE_TRACE_PRINT("TX HIT=0x01 -> KEYA, 0x02->KEYB\r\n");
    }
    else if (hit_report == MOLE_HIT_CODE_KEYB)
    {
        MOLE_TRACE_PRINT("TX HIT=0x02 -> KEYB, 0x01->KEYA\r\n");
    }
    else
    {
        MOLE_TRACE_PRINT("TX HIT=0x%02X\r\n", (unsigned)hit_report);
    }
    BLESendBytes(&hit_report, 1u);
}

static void MoleGame_HandlePacket(const MolePacket *packet)
{
    if (packet == NULL)
    {
        return;
    }

    switch (packet->type)
    {
    case MOLE_PACKET_LED:
    {
        if (s_has_frame && (get_elapsed_ms(s_last_led_apply_ms) < MOLE_LED_MIN_HOLD_MS))
        {
            s_pending_frame = packet->payload.led;
            s_has_pending_frame = 1u;
            MOLE_TRACE_PRINT("DISPLAY HOLD: defer LED frame for >=%ums (elapsed=%ums)\r\n",
                             (unsigned)MOLE_LED_MIN_HOLD_MS,
                             (unsigned)get_elapsed_ms(s_last_led_apply_ms));
        }
        else
        {
            MoleGame_ApplyLedFrame(&packet->payload.led, "direct");
        }
        break;
    }

#if MOLE_ENABLE_RGB16X16
    case MOLE_PACKET_LED16_MONO:
        MoleGame_ApplyLed16MonoFrame(&packet->payload.led16_mono, "direct");
        break;

    case MOLE_PACKET_RGB16_CHUNK:
#if MOLE_ENABLE_RGB16X16_COLOR
        MoleGame_HandleRgb16Chunk(&packet->payload.rgb16_chunk);
#else
        MOLE_TRACE_PRINT("RGB16_CHUNK ignored: color support disabled\r\n");
#endif
        break;
#endif

    case MOLE_PACKET_BRIGHTNESS:
    {
        uint8_t refresh_ok;
        /* Device status report (typically from host echo, apply it) */
        MOLE_TRACE_PRINT("RX BRIGHTNESS status=%u\r\n", (unsigned)packet->payload.brightness_percent);
        MOLE_TRACE_PRINT("DISPLAY STEP1: set brightness=%u\r\n", (unsigned)packet->payload.brightness_percent);
        WS2812B_SetBrightness(packet->payload.brightness_percent);
        if (s_has_display != 0u)
        {
            MOLE_TRACE_PRINT("DISPLAY STEP2: refresh current pixels with new brightness\r\n");
            refresh_ok = WS2812B_Refresh();
            MOLE_TRACE_PRINT("DISPLAY STEP3: refresh result=%s\r\n", refresh_ok ? "OK" : "FAIL");
        }
        else
        {
            MOLE_TRACE_PRINT("DISPLAY STEP2: no last frame, skip redraw until first LED frame\r\n");
        }
        break;
    }

    case MOLE_PACKET_BRIGHTNESS_CMD:
    {
        uint8_t refresh_ok;
        /* Host command: adjust brightness globally */
        s_game_ctx.brightness_percent = packet->payload.brightness_percent;
        MOLE_TRACE_PRINT("RX BRIGHTNESS_CMD value=%u\r\n", (unsigned)s_game_ctx.brightness_percent);
        MOLE_TRACE_PRINT("DISPLAY STEP1: set global brightness=%u\r\n", (unsigned)s_game_ctx.brightness_percent);
        WS2812B_SetBrightness(s_game_ctx.brightness_percent);
        DBG_PRINT("[MOLE] Brightness adjusted to %u%%\n", (unsigned)s_game_ctx.brightness_percent);
        /* Redraw current frame at new brightness */
        if (s_has_display != 0u)
        {
            MOLE_TRACE_PRINT("DISPLAY STEP2: refresh current pixels with new brightness\r\n");
            refresh_ok = WS2812B_Refresh();
            MOLE_TRACE_PRINT("DISPLAY STEP3: refresh result=%s\r\n", refresh_ok ? "OK" : "FAIL");
        }
        else
        {
            MOLE_TRACE_PRINT("DISPLAY STEP2: no last frame, skip redraw until first LED frame\r\n");
        }
        break;
    }

    case MOLE_PACKET_HIT_CONFIG:
        /* Host command: configure hit detection method */
        s_game_ctx.hit_detection_method = packet->payload.hit_detection_method;
        MOLE_TRACE_PRINT("RX HIT_CONFIG method=0x%02X button=%u gsensor=%u\r\n",
                         (unsigned)s_game_ctx.hit_detection_method,
                         (s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_BUTTON) ? 1u : 0u,
                         (s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_GSENSOR) ? 1u : 0u);
        DBG_PRINT("[MOLE] Hit detection method set to 0x%02X (button=%u gsensor=%u)\n",
                  (unsigned)s_game_ctx.hit_detection_method,
                  (s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_BUTTON) ? 1u : 0u,
                  (s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_GSENSOR) ? 1u : 0u);
        break;

    case MOLE_PACKET_NONE:
    default:
        break;
    }
}

static void MoleGame_ProcessBleRawStream(void)
{
    uint8_t raw[MOLE_RAW_READ_CHUNK];
    uint32_t len;

    do
    {
        len = Ble_TakeRawBytes(raw, sizeof(raw));
        for (uint32_t i = 0u; i < len; i++)
        {
            MolePacket packet;
            if (MolePacketParser_PushByte(&s_packet_parser, raw[i], &packet))
            {
                MOLE_TRACE_PRINT("BLE BIN PARSER result=OK type=%s variant=%s\r\n",
                                 MoleGame_PacketTypeToString(packet.type),
                                 MoleGame_PacketVariantToString(packet.variant));
                MoleGame_HandlePacket(&packet);
            }
        }
    } while (len == sizeof(raw));
}

static void MoleGame_ProcessGsensor(uint32_t now_ms)
{
#if MOLE_HIT_GSENSOR_ENABLE
    int16_t axis[3] = {0};
    float mag_g;

    /* Only process G-sensor if enabled in hit detection method */
    if (!(s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_GSENSOR))
    {
        return;
    }

    if ((now_ms - s_last_gsensor_sample_ms) < MOLE_HIT_SAMPLE_INTERVAL_MS)
    {
        return;
    }

    s_last_gsensor_sample_ms = now_ms;
    GsensorReadAxis(axis);
    mag_g = Gsensor_CalcMagnitude_g_from_raw(axis);
    if (MoleHitDetector_ProcessAccelMag(&s_hit_detector, now_ms, mag_g))
    {
        /* Keep legacy payload for G-sensor triggered hit. */
        MoleGame_SendHitReport(MOLE_HIT_CODE_KEYA);
    }
#else
    (void)now_ms;
#endif
}

void MoleGame_Init(void)
{
    DBG_PRINT("[MOLE] Init start\n");
    memset(&s_last_frame, 0, sizeof(s_last_frame));
    memset(&s_pending_frame, 0, sizeof(s_pending_frame));
    s_has_frame = 0u;
    s_has_display = 0u;
    s_has_pending_frame = 0u;
    s_last_gsensor_sample_ms = 0u;
    s_last_led_apply_ms = 0u;

    /* Initialize game context with defaults */
    s_game_ctx.brightness_percent = MOLE_LED_DEFAULT_BRIGHTNESS_PERCENT;
    s_game_ctx.hit_detection_method = MOLE_HIT_METHOD_BUTTON; /* Default: button only */

    MolePacketParser_Init(&s_packet_parser);
#if MOLE_ENABLE_RGB16X16 && MOLE_ENABLE_RGB16X16_COLOR
    MoleGame_ResetRgb16Staging();
#endif
    MoleHitDetector_Init(&s_hit_detector);
    Ble_RawRxReset();
    DBG_PRINT("[MOLE] Parsers reset done\n");
    WS2812B_Init();
    WS2812B_SetBrightness(s_game_ctx.brightness_percent);
    DBG_PRINT("[MOLE] WS2812 brightness set to default=%u\n", (unsigned)s_game_ctx.brightness_percent);
    DBG_PRINT("[MOLE] Hit detection method: button=%u gsensor=%u\n",
              (s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_BUTTON) ? 1u : 0u,
              (s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_GSENSOR) ? 1u : 0u);
    MoleGame_LogBootSixAxisSamples();
    MoleGame_RunWs2812BootSelfTest();
    DBG_PRINT("[MOLE] Init complete\n");
}

void MoleGame_Process(uint32_t now_ms)
{
    MoleGame_CommitPendingLedFrameIfDue();
    MoleGame_ProcessBleRawStream();
    MoleGame_CommitPendingLedFrameIfDue();
    MoleGame_ProcessGsensor(now_ms);
}

void MoleGame_OnButtonEvent(uint32_t now_ms, uint8_t hit_code)
{
    /* Only process button if enabled in hit detection method */
    if (!(s_game_ctx.hit_detection_method & MOLE_HIT_METHOD_BUTTON))
    {
        return;
    }

    if (MoleHitDetector_ProcessButton(&s_hit_detector, now_ms))
    {
        MoleGame_SendHitReport(hit_code);
    }
}

void MoleGame_ShutdownOutputs(void)
{
    WS2812B_Clear();
    (void)WS2812B_Refresh();
}

void MoleGame_ResetFrameState(void)
{
    memset(&s_last_frame, 0, sizeof(s_last_frame));
    memset(&s_pending_frame, 0, sizeof(s_pending_frame));
    s_has_frame = 0u;
    s_has_display = 0u;
    s_has_pending_frame = 0u;
    s_last_led_apply_ms = 0u;
    WS2812B_Clear();
    (void)WS2812B_Refresh();
    MOLE_TRACE_PRINT("DISPLAY RESET: clear output + frame latch\r\n");
}