#include "modbus_timing.h"

#include <stddef.h>

#define MODBUS_HIGH_BAUDRATE_THRESHOLD (19200UL)
#define MODBUS_HIGH_BAUDRATE_FRAME_US (1750UL)
#define MODBUS_HIGH_BAUDRATE_INTERCHAR_US (750UL)

static uint32_t modbus_timing_compute_char_time_us(uint32_t baudrate)
{
    if (baudrate == 0U)
    {
        return 0U;
    }
    uint64_t numerator = 11ULL * 1000000ULL;
    return (uint32_t)((numerator + (uint64_t)baudrate - 1ULL) / (uint64_t)baudrate);
}

static void modbus_timing_apply(modbus_timing_t *timing)
{
    if (timing == NULL)
    {
        return;
    }

    if (timing->baudrate > MODBUS_HIGH_BAUDRATE_THRESHOLD)
    {
        timing->char_time_us = modbus_timing_compute_char_time_us(timing->baudrate);
        timing->frame_timeout_us = MODBUS_HIGH_BAUDRATE_FRAME_US;
        timing->inter_char_timeout_us = MODBUS_HIGH_BAUDRATE_INTERCHAR_US;
    }
    else
    {
        timing->char_time_us = modbus_timing_compute_char_time_us(timing->baudrate);
        timing->frame_timeout_us = (uint32_t)((timing->char_time_us * 35U) / 10U);
        timing->inter_char_timeout_us = (uint32_t)((timing->char_time_us * 15U) / 10U);
    }
}

void modbus_timing_init(modbus_timing_t *timing, uint32_t baudrate)
{
    if (timing == NULL)
    {
        return;
    }

    timing->baudrate = baudrate;
    timing->last_byte_timestamp_us = 0U;
    timing->last_byte_valid = false;
    modbus_timing_apply(timing);
}

void modbus_timing_update(modbus_timing_t *timing, uint32_t baudrate)
{
    if (timing == NULL)
    {
        return;
    }

    timing->baudrate = baudrate;
    modbus_timing_apply(timing);
}

void modbus_timing_mark_rx(modbus_timing_t *timing, uint32_t timestamp_us)
{
    if (timing == NULL)
    {
        return;
    }

    timing->last_byte_timestamp_us = timestamp_us;
    timing->last_byte_valid = true;
}

bool modbus_timing_has_frame_gap(const modbus_timing_t *timing, uint32_t timestamp_us)
{
    if ((timing == NULL) || !timing->last_byte_valid)
    {
        return false;
    }

    return (timestamp_us - timing->last_byte_timestamp_us) >= timing->frame_timeout_us;
}

bool modbus_timing_has_inter_char_timeout(const modbus_timing_t *timing, uint32_t timestamp_us)
{
    if ((timing == NULL) || !timing->last_byte_valid)
    {
        return false;
    }

    return (timestamp_us - timing->last_byte_timestamp_us) >= timing->inter_char_timeout_us;
}

void modbus_timing_reset(modbus_timing_t *timing)
{
    if (timing == NULL)
    {
        return;
    }

    timing->last_byte_timestamp_us = 0U;
    timing->last_byte_valid = false;
}

uint32_t modbus_timing_get_frame_timeout(const modbus_timing_t *timing)
{
    return (timing != NULL) ? timing->frame_timeout_us : 0U;
}

uint32_t modbus_timing_get_char_time_us(const modbus_timing_t *timing)
{
    return (timing != NULL) ? timing->char_time_us : 0U;
}
