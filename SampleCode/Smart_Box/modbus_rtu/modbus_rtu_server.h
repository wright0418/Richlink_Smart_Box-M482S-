#ifndef MODBUS_RTU_SERVER_H
#define MODBUS_RTU_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "modbus_crc.h"
#include "modbus_rtu_protocol.h"
#include "modbus_timing.h"

#define MODBUS_RTU_SERVER_TIMESTAMP_NOW (0xFFFFFFFFu)

typedef uint32_t (*modbus_rtu_timestamp_cb_t)(void *context);
typedef void (*modbus_rtu_server_tx_handler_t)(const uint8_t *data, uint16_t length, void *context);

typedef struct
{
    uint8_t slave_address;
    uint32_t baudrate;
    const modbus_rtu_callbacks_t *callbacks;
    void *callback_context;
    modbus_rtu_server_tx_handler_t tx_handler;
    void *tx_context;
    modbus_rtu_timestamp_cb_t timestamp_callback;
    void *timestamp_context;
    modbus_crc_method_t crc_method;
} modbus_rtu_server_config_t;

typedef struct
{
    uint32_t frames_received;
    uint32_t frames_accepted;
    uint32_t frames_ignored;
    uint32_t crc_errors;
    uint32_t exception_responses;
    uint32_t broadcast_requests;
    uint32_t tx_responses;
    uint32_t rx_buffer_overflows;
    uint32_t rx_frame_dropped;
} modbus_rtu_server_stats_t;

typedef struct
{
    modbus_rtu_server_config_t config;
    modbus_timing_t timing;
    modbus_rtu_server_stats_t stats;
    uint8_t active_buffer[MODBUS_RTU_MAX_ADU_LENGTH];
    uint16_t active_length;
    bool active_overflow;
    uint8_t pending_buffer[MODBUS_RTU_MAX_ADU_LENGTH];
    uint16_t pending_length;
    bool pending_frame;
} modbus_rtu_server_t;

void modbus_rtu_server_init(modbus_rtu_server_t *server, const modbus_rtu_server_config_t *config);
void modbus_rtu_server_reset(modbus_rtu_server_t *server);
void modbus_rtu_server_handle_rx_byte(modbus_rtu_server_t *server, uint8_t byte, uint32_t timestamp_us);
void modbus_rtu_server_poll(modbus_rtu_server_t *server, uint32_t timestamp_us);
void modbus_rtu_server_force_flush(modbus_rtu_server_t *server);
void modbus_rtu_server_set_address(modbus_rtu_server_t *server, uint8_t address);
void modbus_rtu_server_update_baudrate(modbus_rtu_server_t *server, uint32_t baudrate);
uint32_t modbus_rtu_server_get_baudrate(const modbus_rtu_server_t *server);
void modbus_rtu_server_get_stats(const modbus_rtu_server_t *server, modbus_rtu_server_stats_t *stats);
void modbus_rtu_server_clear_stats(modbus_rtu_server_t *server);

#endif
