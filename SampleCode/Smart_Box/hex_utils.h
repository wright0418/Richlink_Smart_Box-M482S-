/**
 * @file hex_utils.h
 * @brief Hexadecimal string conversion utilities
 * @date 2025-10-18
 */

#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <stdint.h>

/**
 * @brief Convert hexadecimal string to byte array
 * @param hex Input hexadecimal string (e.g., "1A2B3C")
 * @param out Output byte array buffer
 * @param max_out Maximum output buffer size
 * @return Number of bytes converted, 0 on error
 * @note Input string must have even length and contain only [0-9A-Fa-f]
 */
uint32_t hex_to_bytes(const char *hex, uint8_t *out, uint32_t max_out);

/**
 * @brief Convert byte array to hexadecimal string
 * @param data Input byte array
 * @param length Input data length
 * @param hex_string Output hexadecimal string buffer
 * @param max_hex Maximum output buffer size (should be at least length*2+1)
 * @return Number of characters written (excluding null terminator), 0 on error
 */
uint32_t bytes_to_hex(const uint8_t *data, uint8_t length, char *hex_string, uint32_t max_hex);

#endif // HEX_UTILS_H
