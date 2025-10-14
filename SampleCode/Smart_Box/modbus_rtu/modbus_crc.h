#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MODBUS_CRC_METHOD_AUTO = 0,
    MODBUS_CRC_METHOD_HARDWARE,
    MODBUS_CRC_METHOD_SOFTWARE
} modbus_crc_method_t;

typedef struct
{
    bool hardware_tested;
    bool hardware_available;
    modbus_crc_method_t active_method;
} modbus_crc_status_t;

uint16_t modbus_crc16_compute(const uint8_t *data, uint16_t length, modbus_crc_method_t method);
bool modbus_crc16_validate_frame(const uint8_t *frame, uint16_t length, modbus_crc_method_t method);
void modbus_crc16_append(uint8_t *frame, uint16_t length, modbus_crc_method_t method);
void modbus_crc_get_status(modbus_crc_status_t *status);
bool modbus_crc_run_self_test(void);

#endif
