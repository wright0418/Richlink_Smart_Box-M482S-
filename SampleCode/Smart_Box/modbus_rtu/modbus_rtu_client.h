#ifndef MODBUS_RTU_CLIENT_H
#define MODBUS_RTU_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "modbus_crc.h"
#include "modbus_rtu_protocol.h"
#include "modbus_timing.h"

#define MODBUS_RTU_CLIENT_MAX_REGISTERS (125U)
#define MODBUS_RTU_CLIENT_MAX_WRITE_MULTIPLE_REGISTERS (123U)

#define MODBUS_RTU_FUNCTION_READ_COILS (0x01U)
#define MODBUS_RTU_FUNCTION_READ_DISCRETE_INPUTS (0x02U)
#define MODBUS_RTU_FUNCTION_READ_HOLDING (0x03U)
#define MODBUS_RTU_FUNCTION_READ_INPUT (0x04U)
#define MODBUS_RTU_FUNCTION_WRITE_SINGLE_COIL (0x05U)
#define MODBUS_RTU_FUNCTION_WRITE_SINGLE (0x06U)
#define MODBUS_RTU_FUNCTION_WRITE_MULTIPLE_COILS (0x0FU)
#define MODBUS_RTU_FUNCTION_WRITE_MULTIPLE (0x10U)

typedef enum
{
    MODBUS_RTU_CLIENT_STATE_IDLE = 0,
    MODBUS_RTU_CLIENT_STATE_WAITING,
    MODBUS_RTU_CLIENT_STATE_COMPLETE,
    MODBUS_RTU_CLIENT_STATE_EXCEPTION,
    MODBUS_RTU_CLIENT_STATE_TIMEOUT,
    MODBUS_RTU_CLIENT_STATE_ERROR
} modbus_rtu_client_state_t;

typedef bool (*modbus_rtu_client_tx_handler_t)(const uint8_t *data, uint16_t length, void *context);
typedef uint32_t (*modbus_rtu_client_timestamp_cb_t)(void *context);

typedef struct
{
    modbus_rtu_client_tx_handler_t tx_handler;
    void *tx_context;
    modbus_rtu_client_timestamp_cb_t timestamp_callback;
    void *timestamp_context;
    uint32_t baudrate;
    modbus_crc_method_t crc_method;
} modbus_rtu_client_config_t;

typedef struct
{
    modbus_rtu_client_config_t config;
    modbus_rtu_client_state_t state;
    uint8_t slave_address;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t quantity;
    uint32_t timeout_us;
    uint32_t request_timestamp_us;
    uint16_t write_value;
    uint8_t response_buffer[MODBUS_RTU_MAX_ADU_LENGTH];
    uint16_t response_length;
    uint16_t expected_length;
    modbus_exception_t exception_code;
    modbus_timing_t timing;
} modbus_rtu_client_t;

bool modbus_rtu_client_init(modbus_rtu_client_t *client, const modbus_rtu_client_config_t *config);
void modbus_rtu_client_reset(modbus_rtu_client_t *client);

bool modbus_rtu_client_start_read_holding(modbus_rtu_client_t *client,
                                          uint8_t slave_address,
                                          uint16_t start_address,
                                          uint16_t quantity,
                                          uint32_t timeout_ms);

bool modbus_rtu_client_start_read_coils(modbus_rtu_client_t *client,
                                        uint8_t slave_address,
                                        uint16_t start_address,
                                        uint16_t quantity,
                                        uint32_t timeout_ms);

bool modbus_rtu_client_start_read_discrete_inputs(modbus_rtu_client_t *client,
                                                  uint8_t slave_address,
                                                  uint16_t start_address,
                                                  uint16_t quantity,
                                                  uint32_t timeout_ms);

bool modbus_rtu_client_start_read_input(modbus_rtu_client_t *client,
                                        uint8_t slave_address,
                                        uint16_t start_address,
                                        uint16_t quantity,
                                        uint32_t timeout_ms);

bool modbus_rtu_client_start_write_single(modbus_rtu_client_t *client,
                                          uint8_t slave_address,
                                          uint16_t register_address,
                                          uint16_t value,
                                          uint32_t timeout_ms);

bool modbus_rtu_client_start_write_single_coil(modbus_rtu_client_t *client,
                                               uint8_t slave_address,
                                               uint16_t coil_address,
                                               uint16_t on_off_value,
                                               uint32_t timeout_ms);

bool modbus_rtu_client_start_write_multiple(modbus_rtu_client_t *client,
                                            uint8_t slave_address,
                                            uint16_t start_address,
                                            uint16_t quantity,
                                            const uint16_t *values,
                                            uint32_t timeout_ms);

bool modbus_rtu_client_start_write_multiple_coils(modbus_rtu_client_t *client,
                                                  uint8_t slave_address,
                                                  uint16_t start_address,
                                                  uint16_t quantity,
                                                  const uint8_t *coil_bytes,
                                                  uint8_t byte_count,
                                                  uint32_t timeout_ms);

// Raw request API for generic/unknown function codes
bool modbus_rtu_client_start_raw(modbus_rtu_client_t *client,
                                 const uint8_t *request_frame,
                                 uint16_t frame_length,
                                 uint32_t timeout_ms);

void modbus_rtu_client_handle_rx_byte(modbus_rtu_client_t *client, uint8_t byte, uint32_t timestamp_us);
void modbus_rtu_client_poll(modbus_rtu_client_t *client, uint32_t timestamp_us);

bool modbus_rtu_client_is_busy(const modbus_rtu_client_t *client);
modbus_rtu_client_state_t modbus_rtu_client_get_state(const modbus_rtu_client_t *client);
uint16_t modbus_rtu_client_get_quantity(const modbus_rtu_client_t *client);
modbus_exception_t modbus_rtu_client_get_exception(const modbus_rtu_client_t *client);

void modbus_rtu_client_copy_response(const modbus_rtu_client_t *client, uint16_t *destination, uint16_t max_registers);
const uint8_t *modbus_rtu_client_get_raw_response(const modbus_rtu_client_t *client, uint16_t *length);

void modbus_rtu_client_clear(modbus_rtu_client_t *client);

#endif
