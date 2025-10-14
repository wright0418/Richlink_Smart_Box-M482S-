#include "modbus_rtu_server.h"

#include <string.h>

#define MODBUS_RTU_ADDRESS_BROADCAST (0x00U)

static uint32_t modbus_rtu_server_now(const modbus_rtu_server_t *server)
{
    if((server != NULL) && (server->config.timestamp_callback != NULL))
    {
        return server->config.timestamp_callback(server->config.timestamp_context);
    }
    return 0U;
}

static void modbus_rtu_server_copy_to_pending(modbus_rtu_server_t *server)
{
    if((server == NULL) || (server->active_length == 0U))
    {
        return;
    }

    if(server->active_overflow)
    {
        server->stats.rx_frame_dropped++;
        server->active_length = 0U;
        server->active_overflow = false;
        modbus_timing_reset(&server->timing);
        return;
    }

    if(server->pending_frame)
    {
        server->stats.rx_frame_dropped++;
        server->active_length = 0U;
        server->active_overflow = false;
        modbus_timing_reset(&server->timing);
        return;
    }

    memcpy(server->pending_buffer, server->active_buffer, server->active_length);
    server->pending_length = server->active_length;
    server->pending_frame = true;
    server->active_length = 0U;
    server->active_overflow = false;
    modbus_timing_reset(&server->timing);
}

static void modbus_rtu_server_emit_response(modbus_rtu_server_t *server, const uint8_t *payload, uint16_t length)
{
    if((server == NULL) || (payload == NULL) || (length == 0U))
    {
        return;
    }

    if(server->config.tx_handler == NULL)
    {
        return;
    }

    uint8_t frame[MODBUS_RTU_MAX_ADU_LENGTH];
    if(length > (MODBUS_RTU_MAX_ADU_LENGTH - 1U))
    {
        return;
    }

    frame[0] = server->config.slave_address;
    memcpy(&frame[1], payload, length);
    modbus_crc16_append(frame, (uint16_t)(length + 1U), server->config.crc_method);
    server->config.tx_handler(frame, (uint16_t)(length + 3U), server->config.tx_context);
    server->stats.tx_responses++;
}

static void modbus_rtu_server_emit_exception(modbus_rtu_server_t *server, uint8_t function_code, modbus_exception_t exception)
{
    if(server == NULL)
    {
        return;
    }

    uint8_t pdu[2];
    pdu[0] = (uint8_t)(function_code | 0x80U);
    pdu[1] = (uint8_t)exception;
    modbus_rtu_server_emit_response(server, pdu, sizeof(pdu));
    server->stats.exception_responses++;
}

static void modbus_rtu_server_process_frame(modbus_rtu_server_t *server, const uint8_t *frame, uint16_t length)
{
    if((server == NULL) || (frame == NULL) || (length < 4U))
    {
        server->stats.frames_ignored++;
        return;
    }

    server->stats.frames_received++;

    if(!modbus_crc16_validate_frame(frame, length, server->config.crc_method))
    {
        server->stats.crc_errors++;
        return;
    }

    uint16_t payload_length = (uint16_t)(length - 2U);
    uint8_t address = frame[0];
    if((address != server->config.slave_address) && (address != MODBUS_RTU_ADDRESS_BROADCAST))
    {
        server->stats.frames_ignored++;
        return;
    }

    bool is_broadcast = (address == MODBUS_RTU_ADDRESS_BROADCAST);
    uint8_t function_code = frame[1];
    if(payload_length < 2U)
    {
        server->stats.frames_ignored++;
        return;
    }
    const uint8_t *pdu_payload = &frame[2];
    uint16_t pdu_length = (uint16_t)(payload_length - 2U);

    server->stats.frames_accepted++;

    uint8_t response_pdu[MODBUS_RTU_MAX_PDU_LENGTH + 1U];
    modbus_rtu_protocol_result_t result = modbus_rtu_protocol_handle_request(server->config.callbacks,
                                                                             server->config.callback_context,
                                                                             function_code,
                                                                             pdu_payload,
                                                                             pdu_length,
                                                                             response_pdu,
                                                                             is_broadcast);

    if(result.status == MODBUS_RTU_STATUS_OK)
    {
        if(is_broadcast || result.suppress_response)
        {
            server->stats.broadcast_requests++;
            return;
        }

        if(result.response_pdu_length > 0U)
        {
            modbus_rtu_server_emit_response(server, response_pdu, result.response_pdu_length);
        }
        return;
    }

    if(result.status == MODBUS_RTU_STATUS_EXCEPTION)
    {
        if(is_broadcast)
        {
            server->stats.broadcast_requests++;
            return;
        }

        modbus_rtu_server_emit_exception(server, function_code, result.exception_code);
    }
}

void modbus_rtu_server_init(modbus_rtu_server_t *server, const modbus_rtu_server_config_t *config)
{
    if((server == NULL) || (config == NULL))
    {
        return;
    }

    memset(server, 0, sizeof(*server));

    server->config = *config;
    if(server->config.crc_method == 0)
    {
        server->config.crc_method = MODBUS_CRC_METHOD_AUTO;
    }

    modbus_timing_init(&server->timing, server->config.baudrate);
}

void modbus_rtu_server_reset(modbus_rtu_server_t *server)
{
    if(server == NULL)
    {
        return;
    }

    server->active_length = 0U;
    server->active_overflow = false;
    server->pending_length = 0U;
    server->pending_frame = false;
    modbus_timing_reset(&server->timing);
}

void modbus_rtu_server_handle_rx_byte(modbus_rtu_server_t *server, uint8_t byte, uint32_t timestamp_us)
{
    if(server == NULL)
    {
        return;
    }

    if((server->active_length > 0U) && modbus_timing_has_frame_gap(&server->timing, timestamp_us))
    {
        modbus_rtu_server_copy_to_pending(server);
    }

    if(server->active_length < MODBUS_RTU_MAX_ADU_LENGTH)
    {
        server->active_buffer[server->active_length++] = byte;
    }
    else
    {
        server->active_overflow = true;
        server->stats.rx_buffer_overflows++;
    }

    modbus_timing_mark_rx(&server->timing, timestamp_us);
}

void modbus_rtu_server_poll(modbus_rtu_server_t *server, uint32_t timestamp_us)
{
    if(server == NULL)
    {
        return;
    }

    uint32_t now = (timestamp_us == MODBUS_RTU_SERVER_TIMESTAMP_NOW) ? modbus_rtu_server_now(server) : timestamp_us;

    if((server->active_length > 0U) && modbus_timing_has_frame_gap(&server->timing, now))
    {
        modbus_rtu_server_copy_to_pending(server);
    }

    if(server->pending_frame)
    {
        modbus_rtu_server_process_frame(server, server->pending_buffer, server->pending_length);
        server->pending_length = 0U;
        server->pending_frame = false;
    }
}

void modbus_rtu_server_force_flush(modbus_rtu_server_t *server)
{
    if(server == NULL)
    {
        return;
    }

    if(server->active_length > 0U)
    {
        modbus_rtu_server_copy_to_pending(server);
    }

    if(server->pending_frame)
    {
        modbus_rtu_server_process_frame(server, server->pending_buffer, server->pending_length);
        server->pending_length = 0U;
        server->pending_frame = false;
    }
}

void modbus_rtu_server_set_address(modbus_rtu_server_t *server, uint8_t address)
{
    if(server == NULL)
    {
        return;
    }

    server->config.slave_address = address;
}

void modbus_rtu_server_update_baudrate(modbus_rtu_server_t *server, uint32_t baudrate)
{
    if(server == NULL)
    {
        return;
    }

    server->config.baudrate = baudrate;
    modbus_timing_update(&server->timing, baudrate);
}

uint32_t modbus_rtu_server_get_baudrate(const modbus_rtu_server_t *server)
{
    if(server == NULL)
    {
        return 0U;
    }
    return server->config.baudrate;
}

void modbus_rtu_server_get_stats(const modbus_rtu_server_t *server, modbus_rtu_server_stats_t *stats)
{
    if((server == NULL) || (stats == NULL))
    {
        return;
    }

    *stats = server->stats;
}

void modbus_rtu_server_clear_stats(modbus_rtu_server_t *server)
{
    if(server == NULL)
    {
        return;
    }

    memset(&server->stats, 0, sizeof(server->stats));
}
