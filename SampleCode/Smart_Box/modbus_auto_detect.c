/**
 * @file modbus_auto_detect.c
 * @brief Modbus RTU Auto-Detection Module Implementation
 * @date 2025-10-19
 */

#include "modbus_auto_detect.h"
#include "uart_rs485_driver.h"
#include <string.h>

// External system time counter
extern volatile uint32_t g_systick_ms;

// Baudrate scan order: 9600 -> 38400 -> 4800 -> 57600 -> 115200
static const uint32_t BAUDRATE_SCAN_ORDER[] = {
    9600,
    38400,
    4800,
    57600,
    115200};

#define BAUDRATE_COUNT (sizeof(BAUDRATE_SCAN_ORDER) / sizeof(BAUDRATE_SCAN_ORDER[0]))
#define QUICK_SCAN_MAX_ID (10U)
#define FULL_SCAN_MAX_ID (255U)

/**
 * @brief Configure UART0 for specific baudrate
 */
static bool configure_uart_for_test(uint32_t baudrate)
{
    // Configure RS485 driver with new baudrate
    uart_rs485_driver_config_t uart_config = {
        .uart = UART0,
        .irq_number = UART0_IRQn,
        .module_clock = UART0_MODULE,
        .baudrate = baudrate,
        .dir_gpio_port = PB,
        .dir_gpio_pin = 14,
        .timestamp_callback = NULL,
        .timestamp_context = NULL};

    uart_rs485_driver_uninit();
    uart_rs485_driver_init(&uart_config);
    return true;
}

/**
 * @brief Get system timestamp in microseconds
 */
static uint32_t get_timestamp_us(void *context)
{
    (void)context;
    return g_systick_ms * 1000U;
}

/**
 * @brief UART TX handler for test client
 */
static bool uart_tx_handler(const uint8_t *data, uint16_t length, void *context)
{
    (void)context;
    return uart_rs485_driver_write(data, length);
}

/**
 * @brief UART RX callback for test client
 */
static void uart_rx_callback(uint8_t byte, uint32_t timestamp_us, void *context)
{
    modbus_rtu_client_t *client = (modbus_rtu_client_t *)context;
    if (client != NULL)
    {
        modbus_rtu_client_handle_rx_byte(client, byte, timestamp_us);
    }
}

/**
 * @brief Test single device ID at current baudrate
 */
static bool test_device_id(
    modbus_rtu_client_t *client,
    uint8_t device_id,
    uint16_t register_address,
    uint32_t timeout_ms)
{
    // Start read holding registers request
    if (!modbus_rtu_client_start_read_holding(client, device_id, register_address, 1, timeout_ms))
    {
        return false;
    }

    // Wait for response or timeout
    uint32_t start_time = g_systick_ms;
    while ((g_systick_ms - start_time) < timeout_ms)
    {
        modbus_rtu_client_state_t state = modbus_rtu_client_get_state(client);

        if (state == MODBUS_RTU_CLIENT_STATE_COMPLETE)
        {
            // Device responded successfully
            return true;
        }
        else if (state == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            // Device responded with exception (still means device exists)
            return true;
        }
        else if (state == MODBUS_RTU_CLIENT_STATE_TIMEOUT || state == MODBUS_RTU_CLIENT_STATE_ERROR)
        {
            // No response or error
            return false;
        }

        // Small delay to prevent busy-waiting
        for (volatile uint32_t i = 0; i < 1000; i++)
            ;
    }

    return false;
}

/**
 * @brief Get default detection configuration
 */
void modbus_auto_detect_get_default_config(modbus_detect_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    config->quick_scan = true;
    config->per_device_timeout_ms = 80;
    config->test_register_address = 0x0000;
    config->max_device_id = QUICK_SCAN_MAX_ID;
}

/**
 * @brief Auto-detect Modbus RTU device
 */
bool modbus_auto_detect_scan(
    modbus_detect_result_t *result,
    const modbus_detect_config_t *config,
    modbus_rtu_client_timestamp_cb_t get_time_ms,
    modbus_detect_progress_callback_t progress_callback,
    void *callback_context)
{
    if (result == NULL || config == NULL)
    {
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(modbus_detect_result_t));
    result->device_found = false;

    uint32_t scan_start_time = g_systick_ms;

    // Determine max device ID to scan
    uint8_t max_device_id = config->quick_scan ? QUICK_SCAN_MAX_ID : config->max_device_id;
    if (max_device_id == 0 || max_device_id > FULL_SCAN_MAX_ID)
    {
        max_device_id = config->quick_scan ? QUICK_SCAN_MAX_ID : FULL_SCAN_MAX_ID;
    }

    // Create temporary Modbus client
    modbus_rtu_client_t test_client;
    modbus_rtu_client_config_t client_config = {
        .tx_handler = uart_tx_handler,
        .tx_context = NULL,
        .timestamp_callback = get_timestamp_us,
        .timestamp_context = NULL,
        .baudrate = 9600, // Will be updated in loop
        .crc_method = MODBUS_CRC_METHOD_AUTO};

    // Scan through baudrates
    for (uint32_t baud_idx = 0; baud_idx < BAUDRATE_COUNT; baud_idx++)
    {
        uint32_t current_baudrate = BAUDRATE_SCAN_ORDER[baud_idx];

        // Configure UART for this baudrate
        if (!configure_uart_for_test(current_baudrate))
        {
            continue;
        }

        // Update client config
        client_config.baudrate = current_baudrate;
        if (!modbus_rtu_client_init(&test_client, &client_config))
        {
            continue;
        }

        // Set RX callback
        uart_rs485_driver_set_rx_callback(uart_rx_callback, &test_client);

        // Scan through device IDs
        for (uint8_t device_id = 1; device_id <= max_device_id; device_id++)
        {
            // Report progress
            if (progress_callback != NULL)
            {
                progress_callback(current_baudrate, device_id, callback_context);
            }

            // Reset client state
            modbus_rtu_client_reset(&test_client);

            // Test this device ID
            if (test_device_id(&test_client, device_id, config->test_register_address, config->per_device_timeout_ms))
            {
                // Device found!
                result->device_found = true;
                result->slave_address = device_id;
                result->baudrate = current_baudrate;
                result->scan_duration_ms = g_systick_ms - scan_start_time;

                // Clean up and return
                uart_rs485_driver_uninit();
                return true;
            }

            // Small delay between tests
            for (volatile uint32_t i = 0; i < 5000; i++)
                ;
        }
    }

    // No device found
    result->scan_duration_ms = g_systick_ms - scan_start_time;
    uart_rs485_driver_uninit();
    return true;
}
