#include "modbus_sensor_manager.h"
#include "uart_rs485_driver.h"
#include <string.h>

// 內部函數聲明
static uint32_t modbus_sensor_get_timestamp_us(void *context);
static bool modbus_sensor_uart_tx_write(const uint8_t *data, uint16_t length, void *context);
static void modbus_sensor_uart_rx_callback(uint8_t byte, uint32_t timestamp_us, void *context);
static void modbus_sensor_handle_client_state(modbus_sensor_manager_t *manager, uint32_t current_time_ms);
static void modbus_sensor_try_start_request(modbus_sensor_manager_t *manager, uint32_t current_time_ms);

// 全域系統時間獲取函數（需要外部實現）
extern volatile uint32_t g_systick_ms;

bool modbus_sensor_manager_init(modbus_sensor_manager_t *manager,
                                const modbus_sensor_config_t *config,
                                modbus_sensor_success_callback_t success_cb,
                                modbus_sensor_error_callback_t error_cb)
{
    if ((manager == NULL) || (config == NULL))
    {
        return false;
    }

    // 清空管理器結構
    memset(manager, 0, sizeof(modbus_sensor_manager_t));

    // 複製配置
    manager->config = *config;
    manager->success_callback = success_cb;
    manager->error_callback = error_cb;

    // 初始化狀態
    manager->state.client_active = false;
    manager->state.sensor_quantity = 0;
    manager->state.last_request_ok = false;
    manager->state.last_exception = MODBUS_EXCEPTION_NONE;
    manager->state.next_poll_ms = 0;
    manager->state.last_response_ms = 0;
    manager->state.consecutive_failures = 0;

    // 配置 UART/RS485 驅動
    uart_rs485_driver_config_t uart_config = {
        .uart = UART0,
        .irq_number = UART0_IRQn,
        .module_clock = UART0_MODULE,
        .baudrate = 9600,
        .dir_gpio_port = PB,
        .dir_gpio_pin = 14,
        .timestamp_callback = modbus_sensor_get_timestamp_us,
        .timestamp_context = NULL};

    // 初始化 UART/RS485 驅動
    uart_rs485_driver_init(&uart_config);
    uart_rs485_driver_set_rx_callback(modbus_sensor_uart_rx_callback, manager);

    // 配置 MODBUS RTU Client
    modbus_rtu_client_config_t client_config = {
        .tx_handler = modbus_sensor_uart_tx_write,
        .tx_context = NULL,
        .timestamp_callback = modbus_sensor_get_timestamp_us,
        .timestamp_context = NULL,
        .baudrate = 9600,
        .crc_method = MODBUS_CRC_METHOD_AUTO};

    // 初始化 MODBUS RTU Client
    manager->state.client_active = modbus_rtu_client_init(&manager->client, &client_config);
    if (manager->state.client_active)
    {
        manager->state.next_poll_ms = g_systick_ms;
        manager->initialized = true;
        return true;
    }

    return false;
}

void modbus_sensor_manager_deinit(modbus_sensor_manager_t *manager)
{
    if (manager == NULL)
    {
        return;
    }

    uart_rs485_driver_uninit();
    manager->initialized = false;
    manager->state.client_active = false;
}

void modbus_sensor_manager_poll(modbus_sensor_manager_t *manager, uint32_t current_time_ms)
{
    if ((manager == NULL) || (!manager->initialized) || (!manager->state.client_active))
    {
        return;
    }

    modbus_sensor_handle_client_state(manager, current_time_ms);
    modbus_sensor_try_start_request(manager, current_time_ms);
}

const modbus_sensor_state_t *modbus_sensor_manager_get_state(const modbus_sensor_manager_t *manager)
{
    if (manager == NULL)
    {
        return NULL;
    }
    return &manager->state;
}

bool modbus_sensor_manager_start_read_now(modbus_sensor_manager_t *manager)
{
    if ((manager == NULL) || (!manager->initialized) || (!manager->state.client_active))
    {
        return false;
    }

    if (modbus_rtu_client_is_busy(&manager->client))
    {
        return false;
    }

    manager->state.next_poll_ms = g_systick_ms; // 強制立即輪詢
    return true;
}

bool modbus_sensor_manager_is_busy(const modbus_sensor_manager_t *manager)
{
    if ((manager == NULL) || (!manager->initialized))
    {
        return false;
    }

    return modbus_rtu_client_is_busy(&manager->client);
}

bool modbus_sensor_manager_update_config(modbus_sensor_manager_t *manager,
                                         const modbus_sensor_config_t *new_config)
{
    if ((manager == NULL) || (new_config == NULL) || (!manager->initialized))
    {
        return false;
    }

    if (modbus_rtu_client_is_busy(&manager->client))
    {
        return false; // 忙碌時不允許更新配置
    }

    manager->config = *new_config;
    return true;
}

// 內部函數實現
static uint32_t modbus_sensor_get_timestamp_us(void *context)
{
    (void)context;
    return g_systick_ms * 1000U;
}

static bool modbus_sensor_uart_tx_write(const uint8_t *data, uint16_t length, void *context)
{
    (void)context;
    if ((data == NULL) || (length == 0U))
    {
        return false;
    }
    return uart_rs485_driver_write(data, length);
}

static void modbus_sensor_uart_rx_callback(uint8_t byte, uint32_t timestamp_us, void *context)
{
    modbus_sensor_manager_t *manager = (modbus_sensor_manager_t *)context;
    if (manager != NULL)
    {
        modbus_rtu_client_handle_rx_byte(&manager->client, byte, timestamp_us);
    }
}

static void modbus_sensor_handle_client_state(modbus_sensor_manager_t *manager, uint32_t current_time_ms)
{
    uint32_t now_us = current_time_ms * 1000U;
    modbus_rtu_client_poll(&manager->client, now_us);

    if (modbus_rtu_client_is_busy(&manager->client))
    {
        return;
    }

    modbus_rtu_client_state_t state = modbus_rtu_client_get_state(&manager->client);
    if (state == MODBUS_RTU_CLIENT_STATE_COMPLETE)
    {
        if ((manager->client.function_code == MODBUS_RTU_FUNCTION_READ_HOLDING) ||
            (manager->client.function_code == MODBUS_RTU_FUNCTION_READ_INPUT))
        {
            uint16_t temp_buffer[MODBUS_RTU_CLIENT_MAX_REGISTERS];
            modbus_rtu_client_copy_response(&manager->client, temp_buffer, MODBUS_RTU_CLIENT_MAX_REGISTERS);
            uint16_t quantity = modbus_rtu_client_get_quantity(&manager->client);

            // 複製到管理器內部緩衝區（限制大小）
            manager->state.sensor_quantity = (quantity > 8) ? 8 : quantity;
            for (uint16_t i = 0; i < manager->state.sensor_quantity; i++)
            {
                manager->state.sensor_registers[i] = temp_buffer[i];
            }
        }
        else
        {
            manager->state.sensor_quantity = 0U;
        }

        manager->state.last_request_ok = true;
        manager->state.last_exception = MODBUS_EXCEPTION_NONE;
        manager->state.last_response_ms = current_time_ms;
        manager->state.consecutive_failures = 0;
        manager->state.next_poll_ms = current_time_ms + manager->config.poll_interval_ms;

        // 調用成功回調
        if (manager->success_callback != NULL)
        {
            manager->success_callback(manager->state.sensor_registers, manager->state.sensor_quantity);
        }

        modbus_rtu_client_clear(&manager->client);
    }
    else if (state != MODBUS_RTU_CLIENT_STATE_IDLE)
    {
        manager->state.last_request_ok = false;

        if (state == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            manager->state.last_exception = modbus_rtu_client_get_exception(&manager->client);
        }
        else
        {
            manager->state.last_exception = MODBUS_EXCEPTION_NONE;
        }

        if (manager->state.consecutive_failures < 0xFFFFFFFFU)
        {
            manager->state.consecutive_failures++;
        }

        manager->state.next_poll_ms = current_time_ms + manager->config.poll_interval_ms;

        // 調用錯誤回調
        if (manager->error_callback != NULL)
        {
            manager->error_callback(manager->state.last_exception, manager->state.consecutive_failures);
        }

        modbus_rtu_client_clear(&manager->client);
    }
}

static void modbus_sensor_try_start_request(modbus_sensor_manager_t *manager, uint32_t current_time_ms)
{
    if (modbus_rtu_client_is_busy(&manager->client))
    {
        return;
    }

    if ((int32_t)(current_time_ms - manager->state.next_poll_ms) < 0)
    {
        return;
    }

    bool started = modbus_rtu_client_start_read_input(&manager->client,
                                                      manager->config.slave_address,
                                                      manager->config.start_address,
                                                      manager->config.register_quantity,
                                                      manager->config.response_timeout_ms);
    if (started)
    {
        manager->state.last_request_ok = false;
        manager->state.last_exception = MODBUS_EXCEPTION_NONE;
    }
    else
    {
        if (manager->state.consecutive_failures < 0xFFFFFFFFU)
        {
            manager->state.consecutive_failures++;
        }
        manager->state.next_poll_ms = current_time_ms + 100U; // 短延遲後重試
    }
}