#include "modbus_rtu_client.h"

#include <string.h>

#include "modbus_crc.h"
#include "modbus_rtu_protocol.h"

#define MODBUS_EXCEPTION_FUNCTION_FLAG (0x80U)

static uint32_t modbus_rtu_client_get_timestamp(const modbus_rtu_client_t *client)
{
    if ((client != NULL) && (client->config.timestamp_callback != NULL))
    {
        return client->config.timestamp_callback(client->config.timestamp_context);
    }
    return 0U;
}

static void modbus_rtu_client_prepare_for_request(modbus_rtu_client_t *client)
{
    client->state = MODBUS_RTU_CLIENT_STATE_WAITING;
    client->response_length = 0U;
    client->expected_length = 0U;
    client->exception_code = MODBUS_EXCEPTION_NONE;
    client->request_timestamp_us = modbus_rtu_client_get_timestamp(client);
    client->timing.last_byte_valid = false;
}

static bool modbus_rtu_client_send_frame(modbus_rtu_client_t *client, uint8_t *frame, uint16_t payload_length)
{
    modbus_crc16_append(frame, payload_length, client->config.crc_method);
    return client->config.tx_handler(frame, (uint16_t)(payload_length + 2U), client->config.tx_context);
}

bool modbus_rtu_client_init(modbus_rtu_client_t *client, const modbus_rtu_client_config_t *config)
{
    if ((client == NULL) || (config == NULL) || (config->tx_handler == NULL) || (config->baudrate == 0U))
    {
        return false;
    }

    memset(client, 0, sizeof(*client));
    client->config = *config;
    client->state = MODBUS_RTU_CLIENT_STATE_IDLE;

    if (client->config.crc_method == 0U)
    {
        client->config.crc_method = MODBUS_CRC_METHOD_AUTO;
    }

    modbus_timing_init(&client->timing, client->config.baudrate);
    return true;
}

void modbus_rtu_client_reset(modbus_rtu_client_t *client)
{
    if (client == NULL)
    {
        return;
    }

    client->state = MODBUS_RTU_CLIENT_STATE_IDLE;
    client->response_length = 0U;
    client->expected_length = 0U;
    client->exception_code = MODBUS_EXCEPTION_NONE;
    client->timeout_us = 0U;
    client->quantity = 0U;
    client->slave_address = 0U;
    client->write_value = 0U;
    modbus_timing_reset(&client->timing);
}

bool modbus_rtu_client_start_read_holding(modbus_rtu_client_t *client,
                                          uint8_t slave_address,
                                          uint16_t start_address,
                                          uint16_t quantity,
                                          uint32_t timeout_ms)
{
    if ((client == NULL) || (quantity == 0U) || (quantity > MODBUS_RTU_CLIENT_MAX_REGISTERS))
    {
        return false;
    }

    if ((client->state == MODBUS_RTU_CLIENT_STATE_WAITING) || (client->config.tx_handler == NULL))
    {
        return false;
    }

    uint8_t frame[MODBUS_RTU_MAX_ADU_LENGTH];
    frame[0] = slave_address;
    frame[1] = MODBUS_RTU_FUNCTION_READ_HOLDING;
    frame[2] = (uint8_t)((start_address >> 8) & 0xFFU);
    frame[3] = (uint8_t)(start_address & 0xFFU);
    frame[4] = (uint8_t)((quantity >> 8) & 0xFFU);
    frame[5] = (uint8_t)(quantity & 0xFFU);

    if (!modbus_rtu_client_send_frame(client, frame, 6U))
    {
        return false;
    }

    client->slave_address = slave_address;
    client->function_code = MODBUS_RTU_FUNCTION_READ_HOLDING;
    client->start_address = start_address;
    client->quantity = quantity;
    client->timeout_us = timeout_ms * 1000U;
    if (client->timeout_us == 0U)
    {
        client->timeout_us = 1000U;
    }

    client->write_value = 0U;

    modbus_timing_update(&client->timing, client->config.baudrate);
    modbus_timing_reset(&client->timing);
    modbus_rtu_client_prepare_for_request(client);
    return true;
}

bool modbus_rtu_client_start_read_input(modbus_rtu_client_t *client,
                                        uint8_t slave_address,
                                        uint16_t start_address,
                                        uint16_t quantity,
                                        uint32_t timeout_ms)
{
    if ((client == NULL) || (quantity == 0U) || (quantity > MODBUS_RTU_CLIENT_MAX_REGISTERS))
    {
        return false;
    }

    if ((client->state == MODBUS_RTU_CLIENT_STATE_WAITING) || (client->config.tx_handler == NULL))
    {
        return false;
    }

    uint8_t frame[MODBUS_RTU_MAX_ADU_LENGTH];
    frame[0] = slave_address;
    frame[1] = MODBUS_RTU_FUNCTION_READ_INPUT;
    frame[2] = (uint8_t)((start_address >> 8) & 0xFFU);
    frame[3] = (uint8_t)(start_address & 0xFFU);
    frame[4] = (uint8_t)((quantity >> 8) & 0xFFU);
    frame[5] = (uint8_t)(quantity & 0xFFU);

    if (!modbus_rtu_client_send_frame(client, frame, 6U))
    {
        return false;
    }

    client->slave_address = slave_address;
    client->function_code = MODBUS_RTU_FUNCTION_READ_INPUT;
    client->start_address = start_address;
    client->quantity = quantity;
    client->timeout_us = timeout_ms * 1000U;
    if (client->timeout_us == 0U)
    {
        client->timeout_us = 1000U;
    }

    client->write_value = 0U;

    modbus_timing_update(&client->timing, client->config.baudrate);
    modbus_timing_reset(&client->timing);
    modbus_rtu_client_prepare_for_request(client);
    return true;
}

bool modbus_rtu_client_start_write_single(modbus_rtu_client_t *client,
                                          uint8_t slave_address,
                                          uint16_t register_address,
                                          uint16_t value,
                                          uint32_t timeout_ms)
{
    if (client == NULL)
    {
        return false;
    }

    if ((client->state == MODBUS_RTU_CLIENT_STATE_WAITING) || (client->config.tx_handler == NULL))
    {
        return false;
    }

    uint8_t frame[MODBUS_RTU_MAX_ADU_LENGTH];
    frame[0] = slave_address;
    frame[1] = MODBUS_RTU_FUNCTION_WRITE_SINGLE;
    frame[2] = (uint8_t)((register_address >> 8) & 0xFFU);
    frame[3] = (uint8_t)(register_address & 0xFFU);
    frame[4] = (uint8_t)((value >> 8) & 0xFFU);
    frame[5] = (uint8_t)(value & 0xFFU);

    if (!modbus_rtu_client_send_frame(client, frame, 6U))
    {
        return false;
    }

    client->slave_address = slave_address;
    client->function_code = MODBUS_RTU_FUNCTION_WRITE_SINGLE;
    client->start_address = register_address;
    client->quantity = 1U;
    client->write_value = value;
    client->timeout_us = timeout_ms * 1000U;
    if (client->timeout_us == 0U)
    {
        client->timeout_us = 1000U;
    }

    modbus_timing_update(&client->timing, client->config.baudrate);
    modbus_timing_reset(&client->timing);
    modbus_rtu_client_prepare_for_request(client);
    return true;
}

bool modbus_rtu_client_start_write_multiple(modbus_rtu_client_t *client,
                                            uint8_t slave_address,
                                            uint16_t start_address,
                                            uint16_t quantity,
                                            const uint16_t *values,
                                            uint32_t timeout_ms)
{
    if ((client == NULL) || (values == NULL) || (quantity == 0U) || (quantity > MODBUS_RTU_CLIENT_MAX_WRITE_MULTIPLE_REGISTERS))
    {
        return false;
    }

    if ((client->state == MODBUS_RTU_CLIENT_STATE_WAITING) || (client->config.tx_handler == NULL))
    {
        return false;
    }

    uint16_t byte_count = (uint16_t)(quantity * 2U);
    uint16_t payload_length = (uint16_t)(7U + byte_count);
    if ((payload_length + 2U) > MODBUS_RTU_MAX_ADU_LENGTH)
    {
        return false;
    }

    uint8_t frame[MODBUS_RTU_MAX_ADU_LENGTH];
    frame[0] = slave_address;
    frame[1] = MODBUS_RTU_FUNCTION_WRITE_MULTIPLE;
    frame[2] = (uint8_t)((start_address >> 8) & 0xFFU);
    frame[3] = (uint8_t)(start_address & 0xFFU);
    frame[4] = (uint8_t)((quantity >> 8) & 0xFFU);
    frame[5] = (uint8_t)(quantity & 0xFFU);
    frame[6] = (uint8_t)byte_count;

    for (uint16_t i = 0U; i < quantity; ++i)
    {
        uint16_t value = values[i];
        frame[7U + (uint16_t)(i * 2U)] = (uint8_t)((value >> 8) & 0xFFU);
        frame[8U + (uint16_t)(i * 2U)] = (uint8_t)(value & 0xFFU);
    }

    if (!modbus_rtu_client_send_frame(client, frame, payload_length))
    {
        return false;
    }

    client->slave_address = slave_address;
    client->function_code = MODBUS_RTU_FUNCTION_WRITE_MULTIPLE;
    client->start_address = start_address;
    client->quantity = quantity;
    client->write_value = 0U;
    client->timeout_us = timeout_ms * 1000U;
    if (client->timeout_us == 0U)
    {
        client->timeout_us = 1000U;
    }

    modbus_timing_update(&client->timing, client->config.baudrate);
    modbus_timing_reset(&client->timing);
    modbus_rtu_client_prepare_for_request(client);
    return true;
}

static void modbus_rtu_client_finish_success(modbus_rtu_client_t *client)
{
    client->state = MODBUS_RTU_CLIENT_STATE_COMPLETE;
}

static void modbus_rtu_client_finish_exception(modbus_rtu_client_t *client, modbus_exception_t exception)
{
    client->exception_code = exception;
    client->state = MODBUS_RTU_CLIENT_STATE_EXCEPTION;
}

static void modbus_rtu_client_finish_error(modbus_rtu_client_t *client, modbus_rtu_client_state_t error_state)
{
    client->state = error_state;
}

void modbus_rtu_client_handle_rx_byte(modbus_rtu_client_t *client, uint8_t byte, uint32_t timestamp_us)
{
    if ((client == NULL) || (client->state != MODBUS_RTU_CLIENT_STATE_WAITING))
    {
        return;
    }

    if (client->response_length > 0U)
    {
        if (modbus_timing_has_frame_gap(&client->timing, timestamp_us))
        {
            client->response_length = 0U;
            client->expected_length = 0U;
        }
    }

    if (client->response_length >= MODBUS_RTU_MAX_ADU_LENGTH)
    {
        modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_ERROR);
        return;
    }

    if ((client->response_length == 0U) && (byte != client->slave_address))
    {
        return;
    }

    client->response_buffer[client->response_length++] = byte;
    modbus_timing_mark_rx(&client->timing, timestamp_us);

    if (client->response_length == 1U)
    {
        return;
    }

    if (client->response_length == 2U)
    {
        uint8_t function_field = client->response_buffer[1];
        if ((function_field != client->function_code) && (function_field != (uint8_t)(client->function_code | MODBUS_EXCEPTION_FUNCTION_FLAG)))
        {
            modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_ERROR);
            return;
        }

        if ((function_field & MODBUS_EXCEPTION_FUNCTION_FLAG) != 0U)
        {
            client->expected_length = 5U;
        }
        else
        {
            if ((client->function_code == MODBUS_RTU_FUNCTION_WRITE_SINGLE) || (client->function_code == MODBUS_RTU_FUNCTION_WRITE_MULTIPLE))
            {
                client->expected_length = 8U;
            }
        }
        return;
    }

    if ((client->expected_length == 0U) && (client->response_length >= 3U))
    {
        uint8_t function_field = client->response_buffer[1];
        if ((function_field & MODBUS_EXCEPTION_FUNCTION_FLAG) != 0U)
        {
            client->expected_length = 5U;
        }
        else if ((client->function_code == MODBUS_RTU_FUNCTION_READ_HOLDING) || (client->function_code == MODBUS_RTU_FUNCTION_READ_INPUT))
        {
            uint8_t byte_count = client->response_buffer[2];
            uint16_t expected_data_bytes = (uint16_t)(client->quantity * 2U);
            if (byte_count != expected_data_bytes)
            {
                modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_ERROR);
                return;
            }
            client->expected_length = (uint16_t)(3U + byte_count + 2U);
        }
    }

    if ((client->expected_length != 0U) && (client->response_length == client->expected_length))
    {
        if (!modbus_crc16_validate_frame(client->response_buffer, client->response_length, client->config.crc_method))
        {
            modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_ERROR);
            return;
        }

        uint8_t function_field = client->response_buffer[1];
        if ((function_field & MODBUS_EXCEPTION_FUNCTION_FLAG) != 0U)
        {
            modbus_exception_t exception = (modbus_exception_t)client->response_buffer[2];
            modbus_rtu_client_finish_exception(client, exception);
        }
        else
        {
            if (function_field == MODBUS_RTU_FUNCTION_WRITE_SINGLE)
            {
                uint16_t resp_address = modbus_rtu_protocol_read_u16(&client->response_buffer[2]);
                uint16_t resp_value = modbus_rtu_protocol_read_u16(&client->response_buffer[4]);
                if ((resp_address != client->start_address) || (resp_value != client->write_value))
                {
                    modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_ERROR);
                    return;
                }
            }
            else if (function_field == MODBUS_RTU_FUNCTION_WRITE_MULTIPLE)
            {
                uint16_t resp_address = modbus_rtu_protocol_read_u16(&client->response_buffer[2]);
                uint16_t resp_quantity = modbus_rtu_protocol_read_u16(&client->response_buffer[4]);
                if ((resp_address != client->start_address) || (resp_quantity != client->quantity))
                {
                    modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_ERROR);
                    return;
                }
            }
            modbus_rtu_client_finish_success(client);
        }
    }
}

void modbus_rtu_client_poll(modbus_rtu_client_t *client, uint32_t timestamp_us)
{
    if ((client == NULL) || (client->state != MODBUS_RTU_CLIENT_STATE_WAITING))
    {
        return;
    }

    if ((client->timeout_us > 0U) && ((timestamp_us - client->request_timestamp_us) >= client->timeout_us))
    {
        modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_TIMEOUT);
        return;
    }

    if ((client->response_length > 0U) && modbus_timing_has_frame_gap(&client->timing, timestamp_us))
    {
        modbus_rtu_client_finish_error(client, MODBUS_RTU_CLIENT_STATE_TIMEOUT);
    }
}

bool modbus_rtu_client_is_busy(const modbus_rtu_client_t *client)
{
    return (client != NULL) && (client->state == MODBUS_RTU_CLIENT_STATE_WAITING);
}

modbus_rtu_client_state_t modbus_rtu_client_get_state(const modbus_rtu_client_t *client)
{
    if (client == NULL)
    {
        return MODBUS_RTU_CLIENT_STATE_ERROR;
    }
    return client->state;
}

uint16_t modbus_rtu_client_get_quantity(const modbus_rtu_client_t *client)
{
    if ((client == NULL) || (client->state != MODBUS_RTU_CLIENT_STATE_COMPLETE))
    {
        return 0U;
    }
    return client->quantity;
}

modbus_exception_t modbus_rtu_client_get_exception(const modbus_rtu_client_t *client)
{
    if (client == NULL)
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }
    return client->exception_code;
}

void modbus_rtu_client_copy_response(const modbus_rtu_client_t *client, uint16_t *destination, uint16_t max_registers)
{
    if ((client == NULL) || (destination == NULL) || (client->state != MODBUS_RTU_CLIENT_STATE_COMPLETE))
    {
        return;
    }

    if ((client->function_code != MODBUS_RTU_FUNCTION_READ_HOLDING) && (client->function_code != MODBUS_RTU_FUNCTION_READ_INPUT))
    {
        return;
    }

    uint16_t quantity = client->quantity;
    if (quantity > max_registers)
    {
        quantity = max_registers;
    }

    const uint8_t *data_ptr = &client->response_buffer[3];
    for (uint16_t i = 0U; i < quantity; ++i)
    {
        destination[i] = modbus_rtu_protocol_read_u16(&data_ptr[i * 2U]);
    }
}

const uint8_t *modbus_rtu_client_get_raw_response(const modbus_rtu_client_t *client, uint16_t *length)
{
    if ((client == NULL) || (length == NULL))
    {
        return NULL;
    }

    *length = client->response_length;
    return client->response_buffer;
}

void modbus_rtu_client_clear(modbus_rtu_client_t *client)
{
    if (client == NULL)
    {
        return;
    }

    client->state = MODBUS_RTU_CLIENT_STATE_IDLE;
    client->response_length = 0U;
    client->expected_length = 0U;
    client->exception_code = MODBUS_EXCEPTION_NONE;
    client->write_value = 0U;
    modbus_timing_reset(&client->timing);
}
