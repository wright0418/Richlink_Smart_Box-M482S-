#include "../protocol/mole_packet.h"

extern int printf(const char *format, ...);

static int s_failed = 0;

#define EXPECT_TRUE(cond, name)            \
    do                                     \
    {                                      \
        if (!(cond))                       \
        {                                  \
            s_failed++;                    \
            printf("[FAIL] %s\n", (name)); \
        }                                  \
        else                               \
        {                                  \
            printf("[PASS] %s\n", (name)); \
        }                                  \
    } while (0)

#define EXPECT_EQ_U8(actual, expected, name)                     \
    do                                                           \
    {                                                            \
        unsigned _a = (unsigned)(uint8_t)(actual);               \
        unsigned _e = (unsigned)(uint8_t)(expected);             \
        if (_a != _e)                                            \
        {                                                        \
            s_failed++;                                          \
            printf("[FAIL] %s: actual=%u expected=%u\n", (name), \
                   _a, _e);                                      \
        }                                                        \
        else                                                     \
        {                                                        \
            printf("[PASS] %s\n", (name));                       \
        }                                                        \
    } while (0)

static uint8_t xor_range(const uint8_t *data, unsigned start, unsigned end_inclusive)
{
    uint8_t checksum = 0u;
    for (unsigned i = start; i <= end_inclusive; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

static void test_led_packet_ePy_variant(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t raw[MOLE_PACKET_LED_LEN] = {
        MOLE_PACKET_HEADER_EPY,
        0x02u,
        0x7Au,
        0x80u,
        0x40u,
        0x20u,
        0x10u,
        0x08u,
        0x04u,
        0x02u,
        0x01u,
        0x00u,
        MOLE_PACKET_FOOTER_EPY};
    raw[11] = xor_range(raw, 1u, 10u);

    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, raw, sizeof(raw), &packet) == 1u,
                "led/ePy: parses packet");
    EXPECT_EQ_U8(packet.type, MOLE_PACKET_LED, "led/ePy: type");
    EXPECT_EQ_U8(packet.variant, MOLE_PACKET_VARIANT_EPY, "led/ePy: variant");
    EXPECT_EQ_U8(packet.payload.led.color, 0x02u, "led/ePy: color");
    EXPECT_EQ_U8(packet.payload.led.target_tag, 0x7Au, "led/ePy: target tag");
    EXPECT_TRUE(MolePacket_IsLedPixelOn(&packet.payload.led, 0u, 0u) == 1u,
                "led/ePy: row MSB maps to col0");
    EXPECT_TRUE(MolePacket_IsLedPixelOn(&packet.payload.led, 0u, 7u) == 0u,
                "led/ePy: row LSB not set for col7");
    EXPECT_TRUE(MolePacket_IsLedPixelOn(&packet.payload.led, 7u, 7u) == 1u,
                "led/ePy: final row LSB maps to col7");
}

static void test_led_packet_app_variant_with_noise(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t raw[] = {
        0x00u,
        0x13u,
        MOLE_PACKET_HEADER_APP,
        0x04u,
        0x55u,
        0xFFu,
        0x00u,
        0xAAu,
        0x55u,
        0x0Fu,
        0xF0u,
        0x81u,
        0x18u,
        0x00u,
        MOLE_PACKET_FOOTER_APP};

    raw[13] = xor_range(raw, 3u, 12u);
    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, raw, sizeof(raw), &packet) == 1u,
                "led/app: skips noise and parses packet");
    EXPECT_EQ_U8(packet.type, MOLE_PACKET_LED, "led/app: type");
    EXPECT_EQ_U8(packet.variant, MOLE_PACKET_VARIANT_APP, "led/app: variant");
    EXPECT_EQ_U8(packet.payload.led.rows[0], 0xFFu, "led/app: first row");
}

static void test_brightness_packet(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t raw[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_APP,
        MOLE_PACKET_TYPE_BRIGHTNESS,
        35u,
        0x00u,
        MOLE_PACKET_FOOTER_APP};
    raw[3] = xor_range(raw, 1u, 2u);

    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, raw, sizeof(raw), &packet) == 1u,
                "brightness: parses packet");
    EXPECT_EQ_U8(packet.type, MOLE_PACKET_BRIGHTNESS, "brightness: type");
    EXPECT_EQ_U8(packet.payload.brightness_percent, 35u, "brightness: value");
}

static void test_brightness_cmd_packet(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t raw[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_EPY,
        MOLE_PACKET_TYPE_BRIGHTNESS_CMD,
        80u,
        0x00u,
        MOLE_PACKET_FOOTER_EPY};
    raw[3] = xor_range(raw, 1u, 2u);

    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, raw, sizeof(raw), &packet) == 1u,
                "brightness_cmd: parses packet");
    EXPECT_EQ_U8(packet.type, MOLE_PACKET_BRIGHTNESS_CMD, "brightness_cmd: type");
    EXPECT_EQ_U8(packet.payload.brightness_percent, 80u, "brightness_cmd: value");
}

static void test_hit_config_packet(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t raw[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_APP,
        MOLE_PACKET_TYPE_HIT_CONFIG,
        (uint8_t)(MOLE_HIT_METHOD_BUTTON | MOLE_HIT_METHOD_GSENSOR),
        0x00u,
        MOLE_PACKET_FOOTER_APP};
    raw[3] = xor_range(raw, 1u, 2u);

    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, raw, sizeof(raw), &packet) == 1u,
                "hit_config: parses packet");
    EXPECT_EQ_U8(packet.type, MOLE_PACKET_HIT_CONFIG, "hit_config: type");
    EXPECT_EQ_U8(packet.payload.hit_detection_method,
                 (uint8_t)(MOLE_HIT_METHOD_BUTTON | MOLE_HIT_METHOD_GSENSOR),
                 "hit_config: method bits");
}

static void test_parser_resync_with_new_command_header(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t stream[] = {
        0x11u,
        0x22u,
        MOLE_PACKET_HEADER_EPY,
        0x09u,
        0x88u,
        MOLE_PACKET_HEADER_EPY,
        MOLE_PACKET_TYPE_HIT_CONFIG,
        MOLE_HIT_METHOD_BUTTON,
        0x00u,
        MOLE_PACKET_FOOTER_EPY};

    stream[8] = xor_range(stream, 6u, 7u);

    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, stream, sizeof(stream), &packet) == 1u,
                "resync: parser restarts at new header");
    EXPECT_EQ_U8(packet.type, MOLE_PACKET_HIT_CONFIG, "resync: final type");
    EXPECT_EQ_U8(packet.payload.hit_detection_method, MOLE_HIT_METHOD_BUTTON, "resync: method");
}

static void test_reject_invalid_packets(void)
{
    MolePacketParser parser;
    MolePacket packet;
    uint8_t bad_checksum[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_APP,
        MOLE_PACKET_TYPE_BRIGHTNESS,
        20u,
        0xFFu,
        MOLE_PACKET_FOOTER_APP};
    uint8_t bad_brightness[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_EPY,
        MOLE_PACKET_TYPE_BRIGHTNESS,
        101u,
        0x00u,
        MOLE_PACKET_FOOTER_EPY};
    uint8_t bad_brightness_cmd[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_EPY,
        MOLE_PACKET_TYPE_BRIGHTNESS_CMD,
        150u,
        0x00u,
        MOLE_PACKET_FOOTER_EPY};
    uint8_t bad_hit_config_checksum[MOLE_PACKET_BRIGHTNESS_LEN] = {
        MOLE_PACKET_HEADER_APP,
        MOLE_PACKET_TYPE_HIT_CONFIG,
        MOLE_HIT_METHOD_GSENSOR,
        0x00u,
        MOLE_PACKET_FOOTER_APP};
    bad_brightness[3] = xor_range(bad_brightness, 1u, 2u);
    bad_brightness_cmd[3] = xor_range(bad_brightness_cmd, 1u, 2u);
    bad_hit_config_checksum[3] = (uint8_t)(xor_range(bad_hit_config_checksum, 1u, 2u) ^ 0xFFu);

    MolePacketParser_Init(&parser);
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, bad_checksum, sizeof(bad_checksum), &packet) == 0u,
                "invalid: rejects bad checksum");
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, bad_brightness, sizeof(bad_brightness), &packet) == 0u,
                "invalid: rejects brightness >100");
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, bad_brightness_cmd, sizeof(bad_brightness_cmd), &packet) == 0u,
                "invalid: rejects brightness_cmd >100");
    EXPECT_TRUE(MolePacketParser_PushBytes(&parser, bad_hit_config_checksum, sizeof(bad_hit_config_checksum), &packet) == 0u,
                "invalid: rejects hit_config bad checksum");
}

int main(void)
{
    printf("Running RL_SPORT Mole packet unit tests...\n");

    test_led_packet_ePy_variant();
    test_led_packet_app_variant_with_noise();
    test_brightness_packet();
    test_brightness_cmd_packet();
    test_hit_config_packet();
    test_parser_resync_with_new_command_header();
    test_reject_invalid_packets();

    if (s_failed == 0)
    {
        printf("\nAll Mole packet tests passed.\n");
        return 0;
    }

    printf("\nMole packet tests failed: %d\n", s_failed);
    return 1;
}
