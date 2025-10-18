#ifndef MESH_MODBUS_AGENT_H
#define MESH_MODBUS_AGENT_H

#include <stdint.h>
#include <stdbool.h>
#include "modbus_rtu/modbus_rtu_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Agent 模式類型
typedef enum
{
    MESH_MODBUS_AGENT_MODE_RL,      // RL Device Mode (帶 header + type)
    MESH_MODBUS_AGENT_MODE_BYPASS   // Bypass Mode (無 header)
} mesh_modbus_agent_mode_t;

// Agent 狀態
typedef enum
{
    AGENT_STATE_IDLE,
    AGENT_STATE_WAITING_MODBUS_RESPONSE,
    AGENT_STATE_RESPONSE_READY,
    AGENT_STATE_ERROR
} agent_state_t;

// Agent 配置
typedef struct
{
    mesh_modbus_agent_mode_t mode;
    uint32_t modbus_timeout_ms;     // MODBUS 回應超時時間
    uint32_t max_response_wait_ms;  // 最大等待時間
} mesh_modbus_agent_config_t;

// Agent 管理結構
typedef struct
{
    mesh_modbus_agent_config_t config;
    agent_state_t state;
    
    // 接收到的 Mesh 資料
    uint8_t mesh_rx_buffer[20];
    uint8_t mesh_rx_length;
    
    // RL Mode 專用：儲存 header 和 type
    uint8_t rl_header[2];
    uint8_t rl_type;
    
    // MODBUS 請求/回應
    uint8_t modbus_request[256];      // MODBUS RTU 最大 ADU 長度
    uint16_t modbus_request_length;
    uint8_t modbus_response[256];     // MODBUS RTU 最大 ADU 長度
    uint16_t modbus_response_length;
    
    // 要回傳給 Mesh 的資料
    uint8_t mesh_tx_buffer[40];       // 足夠容納 header + type + 資料
    uint8_t mesh_tx_length;
    
    // 計時
    uint32_t request_start_ms;
    
    // MODBUS Client
    modbus_rtu_client_t *modbus_client;
    
    // 回調函數
    void (*response_ready_callback)(const uint8_t *data, uint8_t length);
    void (*error_callback)(uint8_t error_code);
    void (*led_flash_red_callback)(uint32_t count);
    
    bool initialized;
} mesh_modbus_agent_t;

// API 函數

/**
 * @brief 初始化 MESH MODBUS Agent
 * 
 * @param agent Agent 管理結構指標
 * @param config Agent 配置
 * @param modbus_client MODBUS RTU Client 指標（已初始化）
 * @param response_cb 回應準備好時的回調函數
 * @param error_cb 錯誤時的回調函數
 * @param led_flash_red_cb 閃紅燈回調函數
 * @return true 初始化成功
 * @return false 初始化失敗
 */
bool mesh_modbus_agent_init(
    mesh_modbus_agent_t *agent,
    const mesh_modbus_agent_config_t *config,
    modbus_rtu_client_t *modbus_client,
    void (*response_cb)(const uint8_t *data, uint8_t length),
    void (*error_cb)(uint8_t error_code),
    void (*led_flash_red_cb)(uint32_t count)
);

/**
 * @brief 反初始化 Agent
 * 
 * @param agent Agent 管理結構指標
 */
void mesh_modbus_agent_deinit(mesh_modbus_agent_t *agent);

/**
 * @brief 處理從 Mesh 收到的資料（已從 Hex String 轉成 bytes）
 * 
 * @param agent Agent 管理結構指標
 * @param data 資料緩衝區
 * @param length 資料長度
 * @return true 處理成功，已發送 MODBUS 請求
 * @return false 處理失敗（格式錯誤或 Agent 忙碌中）
 */
bool mesh_modbus_agent_process_mesh_data(
    mesh_modbus_agent_t *agent,
    const uint8_t *data,
    uint8_t length
);

/**
 * @brief 輪詢處理 Agent 狀態（檢查 MODBUS 回應）
 * 
 * @param agent Agent 管理結構指標
 * @param current_ms 當前時間（毫秒）
 */
void mesh_modbus_agent_poll(mesh_modbus_agent_t *agent, uint32_t current_ms);

/**
 * @brief 獲取 Agent 當前狀態
 * 
 * @param agent Agent 管理結構指標
 * @return agent_state_t 當前狀態
 */
agent_state_t mesh_modbus_agent_get_state(const mesh_modbus_agent_t *agent);

/**
 * @brief 檢查 Agent 是否忙碌中
 * 
 * @param agent Agent 管理結構指標
 * @return true Agent 正在處理請求
 * @return false Agent 閒置
 */
bool mesh_modbus_agent_is_busy(const mesh_modbus_agent_t *agent);

#ifdef __cplusplus
}
#endif

#endif // MESH_MODBUS_AGENT_H
