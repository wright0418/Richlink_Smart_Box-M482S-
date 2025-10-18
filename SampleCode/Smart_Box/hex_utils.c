/**
 * @file hex_utils.c
 * @brief Hexadecimal string conversion utilities implementation
 * @date 2025-10-18
 */

#include "hex_utils.h"
#include <stddef.h>

/**
 * @brief Convert single hex character to nibble value
 * @param c Hex character [0-9A-Fa-f]
 * @return Nibble value (0-15), or -1 on error
 */
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return 10 + (int)(c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (int)(c - 'A');
    return -1;
}

uint32_t hex_to_bytes(const char *hex, uint8_t *out, uint32_t max_out)
{
    if (hex == NULL || out == NULL || max_out == 0)
        return 0;

    // Skip leading whitespace
    while (*hex == ' ' || *hex == '\t')
        hex++;

    // Find end of valid hex string
    const char *p = hex;
    uint32_t n = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
    {
        if (hex_nibble(*p) < 0)
            return 0; // Invalid hex character
        n++;
        p++;
    }

    // Check length: must be >0 and even
    if (n == 0 || (n & 1u) != 0u)
        return 0;

    uint32_t out_len = n / 2u;
    if (out_len > max_out)
        out_len = max_out;

    // Convert hex to bytes
    for (uint32_t i = 0; i < out_len; i++)
    {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return out_len;
}

uint32_t bytes_to_hex(const uint8_t *data, uint8_t length, char *hex_string, uint32_t max_hex)
{
    if (data == NULL || hex_string == NULL || length == 0 || max_hex < (uint32_t)(length * 2 + 1))
        return 0;

    uint32_t hex_idx = 0;

    for (uint8_t i = 0; i < length && hex_idx < max_hex - 1; i++)
    {
        uint8_t b = data[i];
        uint8_t hi = (b >> 4) & 0x0F;
        uint8_t lo = b & 0x0F;

        // High nibble
        if (hi < 10)
            hex_string[hex_idx++] = '0' + hi;
        else
            hex_string[hex_idx++] = 'A' + (hi - 10);

        // Low nibble
        if (lo < 10)
            hex_string[hex_idx++] = '0' + lo;
        else
            hex_string[hex_idx++] = 'A' + (lo - 10);
    }
    hex_string[hex_idx] = '\0';

    return hex_idx;
}
