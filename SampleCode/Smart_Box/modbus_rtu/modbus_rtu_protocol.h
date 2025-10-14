#ifndef MODBUS_RTU_PROTOCOL_H
#define MODBUS_RTU_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define MODBUS_RTU_MAX_PDU_LENGTH (253U)
#define MODBUS_RTU_MAX_ADU_LENGTH (MODBUS_RTU_MAX_PDU_LENGTH + 3U)

typedef enum
{
    MODBUS_EXCEPTION_NONE = 0,
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 1,
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,
    MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE = 4
} modbus_exception_t;

typedef bool (*modbus_rtu_read_registers_cb)(void *context, uint16_t address, uint16_t quantity, uint16_t *buffer);
typedef bool (*modbus_rtu_write_single_cb)(void *context, uint16_t address, uint16_t value);
typedef bool (*modbus_rtu_write_multiple_cb)(void *context, uint16_t address, uint16_t quantity, const uint16_t *buffer);

typedef struct
{
    modbus_rtu_read_registers_cb read_holding;
    modbus_rtu_read_registers_cb read_input;
    modbus_rtu_write_single_cb write_single;
    modbus_rtu_write_multiple_cb write_multiple;
} modbus_rtu_callbacks_t;

typedef enum
{
    MODBUS_RTU_STATUS_OK = 0,
    MODBUS_RTU_STATUS_EXCEPTION,
    MODBUS_RTU_STATUS_ERROR
} modbus_rtu_status_t;

typedef struct
{
    modbus_rtu_status_t status;
    modbus_exception_t exception_code;
    uint16_t response_pdu_length;
    bool suppress_response;
} modbus_rtu_protocol_result_t;

modbus_rtu_protocol_result_t modbus_rtu_protocol_handle_request(const modbus_rtu_callbacks_t *callbacks,
                                                                void *context,
                                                                uint8_t function_code,
                                                                const uint8_t *request_payload,
                                                                uint16_t request_payload_length,
                                                                uint8_t *response_pdu,
                                                                bool is_broadcast);

uint16_t modbus_rtu_protocol_read_u16(const uint8_t *src);
void modbus_rtu_protocol_write_u16(uint8_t *dst, uint16_t value);

#endif
