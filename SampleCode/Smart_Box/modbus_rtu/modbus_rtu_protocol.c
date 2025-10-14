#include "modbus_rtu_protocol.h"

#include <string.h>

#define MODBUS_RTU_MAX_READ_REGISTERS (125U)
#define MODBUS_RTU_MAX_WRITE_REGISTERS (123U)

static modbus_rtu_protocol_result_t modbus_rtu_protocol_make_exception(modbus_exception_t exception)
{
    modbus_rtu_protocol_result_t result;
    result.status = MODBUS_RTU_STATUS_EXCEPTION;
    result.exception_code = exception;
    result.response_pdu_length = 0U;
    result.suppress_response = false;
    return result;
}

static bool modbus_rtu_validate_quantity(uint16_t quantity, uint16_t max_value)
{
    return (quantity >= 1U) && (quantity <= max_value);
}

uint16_t modbus_rtu_protocol_read_u16(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << 8U) | src[1]);
}

void modbus_rtu_protocol_write_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8U);
    dst[1] = (uint8_t)(value & 0xFFU);
}

modbus_rtu_protocol_result_t modbus_rtu_protocol_handle_request(const modbus_rtu_callbacks_t *callbacks,
                                                                void *context,
                                                                uint8_t function_code,
                                                                const uint8_t *request_payload,
                                                                uint16_t request_payload_length,
                                                                uint8_t *response_pdu,
                                                                bool is_broadcast)
{
    modbus_rtu_protocol_result_t result;
    result.status = MODBUS_RTU_STATUS_ERROR;
    result.exception_code = MODBUS_EXCEPTION_NONE;
    result.response_pdu_length = 0U;
    result.suppress_response = false;

    if ((callbacks == NULL) || (response_pdu == NULL))
    {
        return result;
    }

    switch (function_code)
    {
    case 0x03U: /* Read Holding Registers */
    {
        if (is_broadcast)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }

        if (request_payload_length != 4U)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        if (callbacks->read_holding == NULL)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }

        uint16_t start = modbus_rtu_protocol_read_u16(&request_payload[0]);
        uint16_t quantity = modbus_rtu_protocol_read_u16(&request_payload[2]);
        if (!modbus_rtu_validate_quantity(quantity, MODBUS_RTU_MAX_READ_REGISTERS))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        uint16_t words[MODBUS_RTU_MAX_READ_REGISTERS];
        if (!callbacks->read_holding(context, start, quantity, words))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
        }

        response_pdu[0] = function_code;
        response_pdu[1] = (uint8_t)(quantity * 2U);
        for (uint16_t i = 0U; i < quantity; ++i)
        {
            modbus_rtu_protocol_write_u16(&response_pdu[2U + (uint16_t)(i * 2U)], words[i]);
        }
        result.status = MODBUS_RTU_STATUS_OK;
        result.response_pdu_length = (uint16_t)(2U + quantity * 2U);
        return result;
    }
    case 0x04U: /* Read Input Registers */
    {
        if (is_broadcast)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }

        if (request_payload_length != 4U)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        if (callbacks->read_input == NULL)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }

        uint16_t start = modbus_rtu_protocol_read_u16(&request_payload[0]);
        uint16_t quantity = modbus_rtu_protocol_read_u16(&request_payload[2]);
        if (!modbus_rtu_validate_quantity(quantity, MODBUS_RTU_MAX_READ_REGISTERS))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        uint16_t words[MODBUS_RTU_MAX_READ_REGISTERS];
        if (!callbacks->read_input(context, start, quantity, words))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
        }

        response_pdu[0] = function_code;
        response_pdu[1] = (uint8_t)(quantity * 2U);
        for (uint16_t i = 0U; i < quantity; ++i)
        {
            modbus_rtu_protocol_write_u16(&response_pdu[2U + (uint16_t)(i * 2U)], words[i]);
        }
        result.status = MODBUS_RTU_STATUS_OK;
        result.response_pdu_length = (uint16_t)(2U + quantity * 2U);
        return result;
    }
    case 0x06U: /* Write Single Register */
    {
        if (request_payload_length != 4U)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        if (callbacks->write_single == NULL)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }

        uint16_t address = modbus_rtu_protocol_read_u16(&request_payload[0]);
        uint16_t value = modbus_rtu_protocol_read_u16(&request_payload[2]);
        if (!callbacks->write_single(context, address, value))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
        }

        if (is_broadcast)
        {
            result.status = MODBUS_RTU_STATUS_OK;
            result.suppress_response = true;
            return result;
        }

        response_pdu[0] = function_code;
        modbus_rtu_protocol_write_u16(&response_pdu[1], address);
        modbus_rtu_protocol_write_u16(&response_pdu[3], value);
        result.status = MODBUS_RTU_STATUS_OK;
        result.response_pdu_length = 5U;
        return result;
    }
    case 0x10U: /* Write Multiple Registers */
    {
        if (request_payload_length < 5U)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        if (callbacks->write_multiple == NULL)
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }

        uint16_t start = modbus_rtu_protocol_read_u16(&request_payload[0]);
        uint16_t quantity = modbus_rtu_protocol_read_u16(&request_payload[2]);
        uint8_t byte_count = request_payload[4];
        if (!modbus_rtu_validate_quantity(quantity, MODBUS_RTU_MAX_WRITE_REGISTERS))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }
        if (byte_count != (uint8_t)(quantity * 2U))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }
        if (request_payload_length != (uint16_t)(5U + byte_count))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        }

        const uint8_t *values = &request_payload[5];
        uint16_t registers[MODBUS_RTU_MAX_WRITE_REGISTERS];
        for (uint16_t i = 0U; i < quantity; ++i)
        {
            registers[i] = modbus_rtu_protocol_read_u16(&values[i * 2U]);
        }

        if (!callbacks->write_multiple(context, start, quantity, registers))
        {
            return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
        }

        if (is_broadcast)
        {
            result.status = MODBUS_RTU_STATUS_OK;
            result.suppress_response = true;
            return result;
        }

        response_pdu[0] = function_code;
        modbus_rtu_protocol_write_u16(&response_pdu[1], start);
        modbus_rtu_protocol_write_u16(&response_pdu[3], quantity);
        result.status = MODBUS_RTU_STATUS_OK;
        result.response_pdu_length = 5U;
        return result;
    }
    default:
    {
        return modbus_rtu_protocol_make_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
    }
    }
}
