#include "modbus_rtu_test.h"

#include "modbus_crc.h"
#include "modbus_rtu_protocol.h"
#include "modbus_rtu_server.h"

#include <string.h>

#define MODBUS_TEST_REGISTER_COUNT (32U)

typedef struct
{
    uint16_t holding[MODBUS_TEST_REGISTER_COUNT];
    uint16_t input[MODBUS_TEST_REGISTER_COUNT];
} modbus_test_registers_t;

typedef struct
{
    uint8_t buffer[MODBUS_RTU_MAX_ADU_LENGTH];
    uint16_t length;
} modbus_test_tx_capture_t;

static bool modbus_test_read_holding(void *context, uint16_t address, uint16_t quantity, uint16_t *buffer)
{
    modbus_test_registers_t *regs = (modbus_test_registers_t *)context;
    if ((address + quantity) > MODBUS_TEST_REGISTER_COUNT)
    {
        return false;
    }
    for (uint16_t i = 0U; i < quantity; ++i)
    {
        buffer[i] = regs->holding[address + i];
    }
    return true;
}

static bool modbus_test_read_input(void *context, uint16_t address, uint16_t quantity, uint16_t *buffer)
{
    modbus_test_registers_t *regs = (modbus_test_registers_t *)context;
    if ((address + quantity) > MODBUS_TEST_REGISTER_COUNT)
    {
        return false;
    }
    for (uint16_t i = 0U; i < quantity; ++i)
    {
        buffer[i] = regs->input[address + i];
    }
    return true;
}

static bool modbus_test_write_single(void *context, uint16_t address, uint16_t value)
{
    modbus_test_registers_t *regs = (modbus_test_registers_t *)context;
    if (address >= MODBUS_TEST_REGISTER_COUNT)
    {
        return false;
    }
    regs->holding[address] = value;
    return true;
}

static bool modbus_test_write_multiple(void *context, uint16_t address, uint16_t quantity, const uint16_t *buffer)
{
    modbus_test_registers_t *regs = (modbus_test_registers_t *)context;
    if ((address + quantity) > MODBUS_TEST_REGISTER_COUNT)
    {
        return false;
    }
    for (uint16_t i = 0U; i < quantity; ++i)
    {
        regs->holding[address + i] = buffer[i];
    }
    return true;
}

static uint32_t modbus_test_timestamp(void *context)
{
    uint32_t *counter = (uint32_t *)context;
    (*counter) += 500U;
    return *counter;
}

static void modbus_test_tx_capture(const uint8_t *data, uint16_t length, void *context)
{
    modbus_test_tx_capture_t *capture = (modbus_test_tx_capture_t *)context;
    if ((capture == NULL) || (data == NULL) || (length == 0U))
    {
        return;
    }

    if (length > MODBUS_RTU_MAX_ADU_LENGTH)
    {
        length = MODBUS_RTU_MAX_ADU_LENGTH;
    }

    memcpy(capture->buffer, data, length);
    capture->length = length;
}

static bool modbus_rtu_run_server_scenarios(void)
{
    modbus_test_registers_t registers;
    memset(&registers, 0, sizeof(registers));

    modbus_rtu_callbacks_t callbacks;
    callbacks.read_holding = modbus_test_read_holding;
    callbacks.read_input = modbus_test_read_input;
    callbacks.write_single = modbus_test_write_single;
    callbacks.write_multiple = modbus_test_write_multiple;

    uint32_t timestamp_counter = 0U;
    modbus_test_tx_capture_t tx_capture;
    memset(&tx_capture, 0, sizeof(tx_capture));

    modbus_rtu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.slave_address = 0x01U;
    config.baudrate = 9600U;
    config.callbacks = &callbacks;
    config.callback_context = &registers;
    config.tx_handler = modbus_test_tx_capture;
    config.tx_context = &tx_capture;
    config.timestamp_callback = modbus_test_timestamp;
    config.timestamp_context = &timestamp_counter;
    config.crc_method = MODBUS_CRC_METHOD_AUTO;

    modbus_rtu_server_t server;
    modbus_rtu_server_init(&server, &config);

    uint8_t request_write_single[8U] = {0x01U, 0x06U, 0x00U, 0x02U, 0x12U, 0x34U, 0x00U, 0x00U};
    modbus_crc16_append(request_write_single, 6U, MODBUS_CRC_METHOD_AUTO);
    uint16_t total_write_single = 8U;
    uint32_t now = 0U;
    for (uint16_t i = 0U; i < total_write_single; ++i)
    {
        modbus_rtu_server_handle_rx_byte(&server, request_write_single[i], now);
        now += 400U;
    }
    modbus_rtu_server_force_flush(&server);

    if (registers.holding[2] != 0x1234U)
    {
        return false;
    }

    uint8_t request_read_holding[8U] = {0x01U, 0x03U, 0x00U, 0x02U, 0x00U, 0x02U, 0x00U, 0x00U};
    modbus_crc16_append(request_read_holding, 6U, MODBUS_CRC_METHOD_AUTO);
    uint16_t total_read = 8U;
    memset(&tx_capture, 0, sizeof(tx_capture));
    now = 0U;
    for (uint16_t i = 0U; i < total_read; ++i)
    {
        modbus_rtu_server_handle_rx_byte(&server, request_read_holding[i], now);
        now += 400U;
    }
    modbus_rtu_server_force_flush(&server);

    if (tx_capture.length < 7U)
    {
        return false;
    }

    if (tx_capture.buffer[0] != 0x01U || tx_capture.buffer[1] != 0x03U || tx_capture.buffer[2] != 0x04U)
    {
        return false;
    }

    if ((tx_capture.buffer[3] != 0x12U) || (tx_capture.buffer[4] != 0x34U))
    {
        return false;
    }

    uint8_t request_broadcast[13U] = {0x00U, 0x10U, 0x00U, 0x04U, 0x00U, 0x02U, 0x04U, 0x00U, 0x55U, 0x00U, 0x66U, 0x00U, 0x00U};
    modbus_crc16_append(request_broadcast, 11U, MODBUS_CRC_METHOD_AUTO);
    uint16_t total_broadcast = 13U;
    memset(&tx_capture, 0, sizeof(tx_capture));
    now = 0U;
    for (uint16_t i = 0U; i < total_broadcast; ++i)
    {
        modbus_rtu_server_handle_rx_byte(&server, request_broadcast[i], now);
        now += 400U;
    }
    modbus_rtu_server_force_flush(&server);

    if (registers.holding[4] != 0x0055U || registers.holding[5] != 0x0066U)
    {
        return false;
    }

    if (tx_capture.length != 0U)
    {
        return false;
    }

    return true;
}

bool modbus_rtu_run_module_self_test(void)
{
    if (!modbus_crc_run_self_test())
    {
        return false;
    }

    return modbus_rtu_run_server_scenarios();
}
