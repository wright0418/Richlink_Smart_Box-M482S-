#include "mole_packet.h"

#include <stddef.h>
#include <string.h>

#if defined(MOLE_TEST_TRACE_ENABLE) && (MOLE_TEST_TRACE_ENABLE)
extern int printf(const char *format, ...);
#define MOLE_PACKET_TRACE_PRINT(fmt, ...) printf("[MOLE_TEST] " fmt, ##__VA_ARGS__)
#else
#define MOLE_PACKET_TRACE_PRINT(fmt, ...)
#endif

static uint8_t MolePacket_IsHeader(uint8_t byte, MolePacketVariant *variant)
{
    if (byte == MOLE_PACKET_HEADER_EPY)
    {
        if (variant != NULL)
        {
            *variant = MOLE_PACKET_VARIANT_EPY;
        }
        return 1u;
    }

    if (byte == MOLE_PACKET_HEADER_APP)
    {
        if (variant != NULL)
        {
            *variant = MOLE_PACKET_VARIANT_APP;
        }
        return 1u;
    }

    return 0u;
}

static uint8_t MolePacket_GetFooter(MolePacketVariant variant)
{
    return (variant == MOLE_PACKET_VARIANT_APP) ? MOLE_PACKET_FOOTER_APP : MOLE_PACKET_FOOTER_EPY;
}

static uint8_t MolePacket_XorChecksum(const uint8_t *data, uint16_t start, uint16_t end_inclusive)
{
    uint8_t checksum = 0u;

    for (uint16_t i = start; i <= end_inclusive; i++)
    {
        checksum ^= data[i];
    }

    return checksum;
}

static uint8_t MolePacket_BuildLedPacket(const MolePacketParser *parser, MolePacket *out_packet)
{
    uint8_t checksum = MolePacket_XorChecksum(parser->buf, 1u, 10u);
    uint8_t footer = MolePacket_GetFooter(parser->variant);

    if ((parser->buf[11] != checksum) || (parser->buf[12] != footer))
    {
        MOLE_PACKET_TRACE_PRINT("BLE BIN PARSER fail LED chk=0x%02X(exp=0x%02X) footer=0x%02X(exp=0x%02X)\r\n",
                                (unsigned)parser->buf[11],
                                (unsigned)checksum,
                                (unsigned)parser->buf[12],
                                (unsigned)footer);
        return 0u;
    }

    if (out_packet != NULL)
    {
        out_packet->type = MOLE_PACKET_LED;
        out_packet->variant = parser->variant;
        out_packet->payload.led.color = parser->buf[1];
        out_packet->payload.led.target_tag = parser->buf[2];
        memcpy(out_packet->payload.led.rows, &parser->buf[3], MOLE_LED_ROWS);
    }

    return 1u;
}

static uint8_t MolePacket_BuildBrightnessPacket(const MolePacketParser *parser, MolePacket *out_packet)
{
    uint8_t checksum = MolePacket_XorChecksum(parser->buf, 1u, 2u);
    uint8_t brightness = parser->buf[2];
    uint8_t footer = MolePacket_GetFooter(parser->variant);

    if ((parser->buf[3] != checksum) || (parser->buf[4] != footer) || (brightness > 100u))
    {
        MOLE_PACKET_TRACE_PRINT("BLE BIN PARSER fail BRIGHTNESS chk=0x%02X(exp=0x%02X) footer=0x%02X(exp=0x%02X) brightness=%u\r\n",
                                (unsigned)parser->buf[3],
                                (unsigned)checksum,
                                (unsigned)parser->buf[4],
                                (unsigned)footer,
                                (unsigned)brightness);
        return 0u;
    }

    if (out_packet != NULL)
    {
        out_packet->type = MOLE_PACKET_BRIGHTNESS;
        out_packet->variant = parser->variant;
        out_packet->payload.brightness_percent = brightness;
    }

    return 1u;
}

static uint8_t MolePacket_BuildBrightnessCmdPacket(const MolePacketParser *parser, MolePacket *out_packet)
{
    /* Format: [header, 0xC0, brightness_percent, checksum, footer] */
    uint8_t checksum = MolePacket_XorChecksum(parser->buf, 1u, 2u);
    uint8_t brightness = parser->buf[2];
    uint8_t footer = MolePacket_GetFooter(parser->variant);

    if ((parser->buf[3] != checksum) || (parser->buf[4] != footer) || (brightness > 100u))
    {
        MOLE_PACKET_TRACE_PRINT("BLE BIN PARSER fail BRIGHTNESS_CMD chk=0x%02X(exp=0x%02X) footer=0x%02X(exp=0x%02X) brightness=%u\r\n",
                                (unsigned)parser->buf[3],
                                (unsigned)checksum,
                                (unsigned)parser->buf[4],
                                (unsigned)footer,
                                (unsigned)brightness);
        return 0u;
    }

    if (out_packet != NULL)
    {
        out_packet->type = MOLE_PACKET_BRIGHTNESS_CMD;
        out_packet->variant = parser->variant;
        out_packet->payload.brightness_percent = brightness;
    }

    return 1u;
}

static uint8_t MolePacket_BuildHitConfigPacket(const MolePacketParser *parser, MolePacket *out_packet)
{
    /* Format: [header, 0xC1, method_bits, checksum, footer] */
    uint8_t checksum = MolePacket_XorChecksum(parser->buf, 1u, 2u);
    uint8_t method_bits = parser->buf[2];
    uint8_t footer = MolePacket_GetFooter(parser->variant);

    if ((parser->buf[3] != checksum) || (parser->buf[4] != footer))
    {
        MOLE_PACKET_TRACE_PRINT("BLE BIN PARSER fail HIT_CONFIG chk=0x%02X(exp=0x%02X) footer=0x%02X(exp=0x%02X) method=0x%02X\r\n",
                                (unsigned)parser->buf[3],
                                (unsigned)checksum,
                                (unsigned)parser->buf[4],
                                (unsigned)footer,
                                (unsigned)method_bits);
        return 0u;
    }

    if (out_packet != NULL)
    {
        out_packet->type = MOLE_PACKET_HIT_CONFIG;
        out_packet->variant = parser->variant;
        out_packet->payload.hit_detection_method = method_bits;
    }

    return 1u;
}

void MolePacketParser_Init(MolePacketParser *parser)
{
    MolePacketParser_Reset(parser);
}

void MolePacketParser_Reset(MolePacketParser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->len = 0u;
    parser->expected_len = 0u;
    parser->variant = MOLE_PACKET_VARIANT_EPY;
}

uint8_t MolePacketParser_PushByte(MolePacketParser *parser, uint8_t byte, MolePacket *out_packet)
{
    MolePacketVariant variant = MOLE_PACKET_VARIANT_EPY;

    if (parser == NULL)
    {
        return 0u;
    }

    if (out_packet != NULL)
    {
        out_packet->type = MOLE_PACKET_NONE;
    }

    if (parser->len == 0u)
    {
        if (!MolePacket_IsHeader(byte, &variant))
        {
            return 0u;
        }

        parser->variant = variant;
        parser->buf[0] = byte;
        parser->len = 1u;
        parser->expected_len = 0u;
        return 0u;
    }

    /* IMPORTANT:
       Do not blindly resync on header-like bytes while already receiving a packet.
       Payload/checksum can legally contain 0xAA or 0xFD (e.g. LED checksum=0xFD),
       and resetting here would drop valid frames.

       Only allow header-resync in the narrow "double header" case where we have
       just captured the first header byte and have not yet determined type/length. */
    if ((parser->len == 1u) && (parser->expected_len == 0u) && MolePacket_IsHeader(byte, &variant))
    {
        parser->variant = variant;
        parser->buf[0] = byte;
        parser->len = 1u;
        parser->expected_len = 0u;
        return 0u;
    }

    if (parser->len >= MOLE_PACKET_RX_CACHE_SIZE)
    {
        MOLE_PACKET_TRACE_PRINT("BLE BIN PARSER overflow reset len=%u\r\n", (unsigned)parser->len);
        MolePacketParser_Reset(parser);
        return 0u;
    }

    parser->buf[parser->len++] = byte;

    if (parser->len == 2u)
    {
        /* Determine expected packet length based on type byte */
        uint8_t type_byte = parser->buf[1];
        if ((type_byte == MOLE_PACKET_TYPE_BRIGHTNESS) ||
            (type_byte == MOLE_PACKET_TYPE_BRIGHTNESS_CMD) ||
            (type_byte == MOLE_PACKET_TYPE_HIT_CONFIG))
        {
            parser->expected_len = MOLE_PACKET_BRIGHTNESS_LEN;
        }
        else
        {
            parser->expected_len = MOLE_PACKET_LED_LEN;
        }
    }

    if ((parser->expected_len == 0u) || (parser->len < parser->expected_len))
    {
        return 0u;
    }

    uint8_t ok = 0u;
    uint8_t type_byte = parser->buf[1];
    uint16_t rx_len = parser->len;
    MolePacketVariant rx_variant = parser->variant;
    (void)rx_len;
    (void)rx_variant;
    if (parser->expected_len == MOLE_PACKET_BRIGHTNESS_LEN)
    {
        if (type_byte == MOLE_PACKET_TYPE_BRIGHTNESS)
        {
            ok = MolePacket_BuildBrightnessPacket(parser, out_packet);
        }
        else if (type_byte == MOLE_PACKET_TYPE_BRIGHTNESS_CMD)
        {
            ok = MolePacket_BuildBrightnessCmdPacket(parser, out_packet);
        }
        else if (type_byte == MOLE_PACKET_TYPE_HIT_CONFIG)
        {
            ok = MolePacket_BuildHitConfigPacket(parser, out_packet);
        }
    }
    else
    {
        ok = MolePacket_BuildLedPacket(parser, out_packet);
    }

    if (ok == 0u)
    {
        MOLE_PACKET_TRACE_PRINT("BLE BIN PARSER drop type=0x%02X len=%u variant=%u\r\n",
                                (unsigned)type_byte,
                                (unsigned)rx_len,
                                (unsigned)rx_variant);
    }

    MolePacketParser_Reset(parser);
    return ok;
}

uint8_t MolePacketParser_PushBytes(MolePacketParser *parser, const uint8_t *data, uint32_t len, MolePacket *out_packet)
{
    if ((parser == NULL) || (data == NULL))
    {
        return 0u;
    }

    for (uint32_t i = 0u; i < len; i++)
    {
        if (MolePacketParser_PushByte(parser, data[i], out_packet))
        {
            return 1u;
        }
    }

    return 0u;
}

uint8_t MolePacket_IsLedPixelOn(const MoleLedFrame *frame, uint8_t row, uint8_t col)
{
    uint8_t mask;

    if ((frame == NULL) || (row >= MOLE_LED_ROWS) || (col >= 8u))
    {
        return 0u;
    }

    mask = (uint8_t)(0x80u >> col);
    return (frame->rows[row] & mask) ? 1u : 0u;
}
