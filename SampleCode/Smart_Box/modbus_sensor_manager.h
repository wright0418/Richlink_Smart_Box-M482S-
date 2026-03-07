#ifndef MODBUS_SENSOR_MANAGER_H
#define MODBUS_SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "modbus_rtu_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MODBUS Sensor Manager 配置結構
    typedef struct
    {
        uint8_t slave_address;
        uint16_t start_address;
        uint16_t register_quantity;
        uint32_t poll_interval_ms;
        uint32_t response_timeout_ms;
        uint32_t failure_threshold;
    } modbus_sensor_config_t;

    // MODBUS Sensor Manager 狀態結構
    typedef struct
    {
        uint16_t sensor_registers[8]; // 優化：減少記憶體使用，最多8個暫存器
        uint16_t sensor_quantity;
        bool last_request_ok;
        modbus_exception_t last_exception;
        uint32_t last_response_ms;
        uint32_t consecutive_failures;
        uint32_t next_poll_ms;
        bool client_active;
    } modbus_sensor_state_t;

    // 回調函數類型定義
    typedef void (*modbus_sensor_success_callback_t)(const uint16_t *registers, uint16_t quantity);
    typedef void (*modbus_sensor_error_callback_t)(modbus_exception_t exception, uint32_t consecutive_failures);

    // MODBUS Sensor Manager 控制結構
    typedef struct
    {
        modbus_sensor_config_t config;
        modbus_sensor_state_t state;
        modbus_rtu_client_t client;
        modbus_sensor_success_callback_t success_callback;
        modbus_sensor_error_callback_t error_callback;
        bool initialized;
    } modbus_sensor_manager_t;

    // 初始化函數
    bool modbus_sensor_manager_init(modbus_sensor_manager_t *manager,
                                    const modbus_sensor_config_t *config,
                                    modbus_sensor_success_callback_t success_cb,
                                    modbus_sensor_error_callback_t error_cb);

    // 反初始化函數
    void modbus_sensor_manager_deinit(modbus_sensor_manager_t *manager);

    // 主輪詢函數（需要在主迴圈中調用）
    void modbus_sensor_manager_poll(modbus_sensor_manager_t *manager, uint32_t current_time_ms);

    // 獲取當前狀態
    const modbus_sensor_state_t *modbus_sensor_manager_get_state(const modbus_sensor_manager_t *manager);

    // 強制開始新的讀取請求
    bool modbus_sensor_manager_start_read_now(modbus_sensor_manager_t *manager);

    // 檢查是否正在通訊中
    bool modbus_sensor_manager_is_busy(const modbus_sensor_manager_t *manager);

    // 更新配置(僅在非忙碌狀態下可用)
    bool modbus_sensor_manager_update_config(modbus_sensor_manager_t *manager,
                                             const modbus_sensor_config_t *new_config);

    // 動態設定 baudrate (會重新初始化 UART)
    bool modbus_sensor_manager_set_baudrate(modbus_sensor_manager_t *manager,
                                            uint32_t new_baudrate);

    // 動態設定 slave address
    bool modbus_sensor_manager_set_slave_address(modbus_sensor_manager_t *manager,
                                                 uint8_t new_slave_address);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_SENSOR_MANAGER_H