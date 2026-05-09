#ifndef MOLE_PACKET_H
#define MOLE_PACKET_H

#include <stdint.h>

#ifndef MOLE_LED_ROWS
#define MOLE_LED_ROWS 8u
#endif

#ifndef MOLE_PACKET_RX_CACHE_SIZE
#define MOLE_PACKET_RX_CACHE_SIZE 256u
#endif

#define MOLE_PACKET_LED_LEN 13u
#define MOLE_PACKET_BRIGHTNESS_LEN 5u
#define MOLE_PACKET_HEADER_EPY 0xAAu
#define MOLE_PACKET_FOOTER_EPY 0x55u
#define MOLE_PACKET_HEADER_APP 0xFDu
#define MOLE_PACKET_FOOTER_APP 0xFEu
#define MOLE_PACKET_TYPE_BRIGHTNESS 0xB0u
#define MOLE_PACKET_TYPE_BRIGHTNESS_CMD 0xC0u
#define MOLE_PACKET_TYPE_HIT_CONFIG 0xC1u

typedef enum
{
    MOLE_PACKET_VARIANT_EPY = 0,
    MOLE_PACKET_VARIANT_APP = 1
} MolePacketVariant;

typedef enum
{
    MOLE_PACKET_NONE = 0,
    MOLE_PACKET_LED,
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

#endif /* MOLE_PACKET_H */
