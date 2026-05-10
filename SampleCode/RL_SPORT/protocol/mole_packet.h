#ifndef MOLE_PACKET_H
#define MOLE_PACKET_H

#include <stdint.h>

#if defined(__has_include)
#if __has_include("../project_config.h")
#include "../project_config.h"
#endif
#endif

#ifndef MOLE_LED_ROWS
#define MOLE_LED_ROWS 8u
#endif

#ifndef MOLE_PACKET_RX_CACHE_SIZE
#define MOLE_PACKET_RX_CACHE_SIZE 256u
#endif

#ifndef MOLE_ENABLE_RGB16X16
#define MOLE_ENABLE_RGB16X16 0u
#endif

#ifndef MOLE_ENABLE_RGB16X16_COLOR
#define MOLE_ENABLE_RGB16X16_COLOR 0u
#endif

#ifndef MOLE_RGB16_ROWS
#define MOLE_RGB16_ROWS 16u
#endif

#ifndef MOLE_RGB16_COLS
#define MOLE_RGB16_COLS 16u
#endif

#ifndef MOLE_RGB16_LED_COUNT
#define MOLE_RGB16_LED_COUNT (MOLE_RGB16_ROWS * MOLE_RGB16_COLS)
#endif

#ifndef MOLE_RGB16_COLOR_BYTES
#define MOLE_RGB16_COLOR_BYTES (MOLE_RGB16_LED_COUNT * 3u)
#endif

#ifndef MOLE_RGB16_CHUNK_PAYLOAD_MAX
#define MOLE_RGB16_CHUNK_PAYLOAD_MAX 10u
#endif

#define MOLE_PACKET_LED_LEN 13u
#define MOLE_PACKET_BRIGHTNESS_LEN 5u
#define MOLE_PACKET_LED16_MONO_LEN (1u + 1u + 1u + 1u + (MOLE_RGB16_ROWS * 2u) + 1u + 1u)
#define MOLE_PACKET_RGB16_CHUNK_OVERHEAD_LEN 10u
#define MOLE_PACKET_RGB16_CHUNK_MIN_LEN MOLE_PACKET_RGB16_CHUNK_OVERHEAD_LEN
#define MOLE_PACKET_RGB16_CHUNK_MAX_LEN (MOLE_PACKET_RGB16_CHUNK_OVERHEAD_LEN + MOLE_RGB16_CHUNK_PAYLOAD_MAX)
#define MOLE_PACKET_HEADER_EPY 0xAAu
#define MOLE_PACKET_FOOTER_EPY 0x55u
#define MOLE_PACKET_HEADER_APP 0xFDu
#define MOLE_PACKET_FOOTER_APP 0xFEu
#define MOLE_PACKET_TYPE_BRIGHTNESS 0xB0u
#define MOLE_PACKET_TYPE_BRIGHTNESS_CMD 0xC0u
#define MOLE_PACKET_TYPE_HIT_CONFIG 0xC1u
#define MOLE_PACKET_TYPE_LED16_MONO 0xD0u
#define MOLE_PACKET_TYPE_RGB16_CHUNK 0xD1u

#define MOLE_RGB16_CHUNK_OP_START 0x01u
#define MOLE_RGB16_CHUNK_OP_DATA 0x02u
#define MOLE_RGB16_CHUNK_OP_COMMIT 0x03u
#define MOLE_RGB16_CHUNK_OP_CANCEL 0x04u

typedef enum
{
    MOLE_PACKET_VARIANT_EPY = 0,
    MOLE_PACKET_VARIANT_APP = 1
} MolePacketVariant;

typedef enum
{
    MOLE_PACKET_NONE = 0,
    MOLE_PACKET_LED,
    MOLE_PACKET_LED16_MONO,
    MOLE_PACKET_RGB16_CHUNK,
    MOLE_PACKET_BRIGHTNESS,
    MOLE_PACKET_BRIGHTNESS_CMD,
    MOLE_PACKET_HIT_CONFIG
} MolePacketType;

typedef struct
{
    uint8_t color;
    uint8_t target_tag;
    uint8_t rows[MOLE_LED_ROWS];
} MoleLedFrame;

typedef struct
{
    uint8_t color;
    uint8_t target_tag;
    uint8_t rows[MOLE_RGB16_ROWS][2]; /* big-endian row bits: bit15 maps to col0 */
} MoleLedFrame16Mono;

typedef struct
{
    uint8_t op;
    uint8_t frame_id;
    uint8_t chunk_index;
    uint16_t offset;
    uint8_t payload_len;
    uint8_t payload[MOLE_RGB16_CHUNK_PAYLOAD_MAX]; /* RGB byte stream: R,G,B,R,G,B... */
} MoleRgb16Chunk;

/* Hit detection method bits (can be ORed together) */
#define MOLE_HIT_METHOD_BUTTON 0x01u
#define MOLE_HIT_METHOD_GSENSOR 0x02u

typedef struct
{
    MolePacketType type;
    MolePacketVariant variant;
    union
    {
        MoleLedFrame led;
#if MOLE_ENABLE_RGB16X16
        MoleLedFrame16Mono led16_mono;
        MoleRgb16Chunk rgb16_chunk;
#endif
        uint8_t brightness_percent;   /* For both BRIGHTNESS and BRIGHTNESS_CMD */
        uint8_t hit_detection_method; /* For HIT_CONFIG: bitmask of methods */
    } payload;
} MolePacket;

typedef struct
{
    uint8_t buf[MOLE_PACKET_RX_CACHE_SIZE];
    uint16_t len;
    uint16_t expected_len;
    MolePacketVariant variant;
} MolePacketParser;

void MolePacketParser_Init(MolePacketParser *parser);
void MolePacketParser_Reset(MolePacketParser *parser);
uint8_t MolePacketParser_PushByte(MolePacketParser *parser, uint8_t byte, MolePacket *out_packet);
uint8_t MolePacketParser_PushBytes(MolePacketParser *parser, const uint8_t *data, uint32_t len, MolePacket *out_packet);
uint8_t MolePacket_IsLedPixelOn(const MoleLedFrame *frame, uint8_t row, uint8_t col);
uint8_t MolePacket_IsLed16PixelOn(const MoleLedFrame16Mono *frame, uint8_t row, uint8_t col);

#endif /* MOLE_PACKET_H */
