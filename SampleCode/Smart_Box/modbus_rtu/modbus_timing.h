#ifndef MODBUS_TIMING_H
#define MODBUS_TIMING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t baudrate;
    uint32_t char_time_us;
    uint32_t inter_char_timeout_us;
    uint32_t frame_timeout_us;
    uint32_t last_byte_timestamp_us;
    bool last_byte_valid;
} modbus_timing_t;

void modbus_timing_init(modbus_timing_t *timing, uint32_t baudrate);
void modbus_timing_update(modbus_timing_t *timing, uint32_t baudrate);
void modbus_timing_mark_rx(modbus_timing_t *timing, uint32_t timestamp_us);
bool modbus_timing_has_frame_gap(const modbus_timing_t *timing, uint32_t timestamp_us);
bool modbus_timing_has_inter_char_timeout(const modbus_timing_t *timing, uint32_t timestamp_us);
void modbus_timing_reset(modbus_timing_t *timing);
uint32_t modbus_timing_get_frame_timeout(const modbus_timing_t *timing);
uint32_t modbus_timing_get_char_time_us(const modbus_timing_t *timing);

#endif
