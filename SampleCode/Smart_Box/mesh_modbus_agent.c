#include "mesh_modbus_agent.h"
#include "modbus_rtu/modbus_crc.h"
#include "led_indicator.h"

// 定義 NULL（避免依賴標準庫）
#ifndef NULL
#define NULL ((void *)0)
#endif

// 簡易 memcpy 實作
static void *my_memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
    {
        *d++ = *s++;
    }
    return dest;
}

// 簡易 memset 實作
static void *my_memset(void *s, int c, unsigned int n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--)
    {
        *p++ = (unsigned char)c;
    }
    return s;
}

// RL Mode 的 Header 定義
#define RL_HEADER_BYTE1 0x82
#define RL_HEADER_BYTE2 0x76

// RL Mode 的 Type 定義
#define RL_TYPE_GET 0x01
#define RL_TYPE_SET 0x02
#define RL_TYPE_RTU 0x03

// 錯誤碼定義
#define ERROR_CODE_CRC_FAILED 0xFE
#define ERROR_CODE_BUSY 0xFD // 忙碌中，無法處理請求

// 內部函數聲明
static bool parse_rl_mode_data(mesh_modbus_agent_t *agent, const uint8_t *data, uint8_t length);
static bool parse_bypass_mode_data(mesh_modbus_agent_t *agent, const uint8_t *data, uint8_t length);
static bool send_modbus_request(mesh_modbus_agent_t *agent);
static void handle_modbus_response(mesh_modbus_agent_t *agent, uint32_t current_ms);
static void prepare_mesh_response(mesh_modbus_agent_t *agent, bool crc_ok);
static uint16_t calculate_crc16(const uint8_t *data, uint16_t length);

// 外部時間函數（由 main.c 提供）
extern uint32_t get_system_time_ms(void);

bool mesh_modbus_agent_init(
    mesh_modbus_agent_t *agent,
    const mesh_modbus_agent_config_t *config,
    modbus_rtu_client_t *modbus_client,
    void (*response_cb)(const uint8_t *data, uint8_t length),
    void (*error_cb)(uint8_t error_code),
    void (*led_flash_red_cb)(uint32_t count))
{
    if ((agent == NULL) || (config == NULL) || (modbus_client == NULL))
    {
        return false;
    }

    // 清空結構
    my_memset(agent, 0, sizeof(mesh_modbus_agent_t));

    // 複製配置
    agent->config = *config;
    agent->modbus_client = modbus_client;
    agent->response_ready_callback = response_cb;
    agent->error_callback = error_cb;
    agent->led_flash_red_callback = led_flash_red_cb;

    // 初始化狀態
    agent->state = AGENT_STATE_IDLE;
    agent->initialized = true;

    return true;
}

void mesh_modbus_agent_deinit(mesh_modbus_agent_t *agent)
{
    if (agent == NULL)
    {
        return;
    }

    agent->initialized = false;
    agent->state = AGENT_STATE_IDLE;
}

bool mesh_modbus_agent_process_mesh_data(
    mesh_modbus_agent_t *agent,
    const uint8_t *data,
    uint8_t length)
{
    if ((agent == NULL) || (!agent->initialized) || (data == NULL) || (length == 0))
    {
        return false;
    }

    // 檢查 Agent 是否忙碌中
    if (agent->state != AGENT_STATE_IDLE)
    {
        return false;
    }

    // 儲存接收到的資料
    if (length > sizeof(agent->mesh_rx_buffer))
    {
        length = sizeof(agent->mesh_rx_buffer);
    }
    my_memcpy(agent->mesh_rx_buffer, data, length);
    agent->mesh_rx_length = length;

    // 根據模式解析資料
    bool parse_ok = false;
    if (agent->config.mode == MESH_MODBUS_AGENT_MODE_RL)
    {
        parse_ok = parse_rl_mode_data(agent, data, length);
    }
    else // BYPASS mode
    {
        parse_ok = parse_bypass_mode_data(agent, data, length);
    }

    if (!parse_ok)
    {
        return false;
    }

    // 發送 MODBUS 請求
    if (!send_modbus_request(agent))
    {
        // 立即回應「忙碌中」錯誤給 mesh 主機
        if (agent->config.mode == MESH_MODBUS_AGENT_MODE_RL)
        {
            // RL Mode: 組裝 header + type + 錯誤碼
            agent->mesh_tx_length = 0;
            agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->rl_header[0];
            agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->rl_header[1];
            agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->rl_type;
            agent->mesh_tx_buffer[agent->mesh_tx_length++] = ERROR_CODE_BUSY;

            if (agent->response_ready_callback != NULL)
            {
                agent->response_ready_callback(agent->mesh_tx_buffer, agent->mesh_tx_length);
            }
        }
        else // BYPASS mode
        {
            // Bypass Mode: 直接回應錯誤碼
            agent->mesh_tx_length = 1;
            agent->mesh_tx_buffer[0] = ERROR_CODE_BUSY;

            if (agent->response_ready_callback != NULL)
            {
                agent->response_ready_callback(agent->mesh_tx_buffer, agent->mesh_tx_length);
            }
        }

        return false;
    }

    // 更新狀態
    agent->state = AGENT_STATE_WAITING_MODBUS_RESPONSE;
    agent->request_start_ms = get_system_time_ms();

    return true;
}

void mesh_modbus_agent_poll(mesh_modbus_agent_t *agent, uint32_t current_ms)
{
    if ((agent == NULL) || (!agent->initialized))
    {
        return;
    }

    // 重要：定期呼叫 modbus_rtu_client_poll 來檢查 timeout
    // 將 ms 轉換為 us
    uint32_t current_us = current_ms * 1000U;
    modbus_rtu_client_poll(agent->modbus_client, current_us);

    // 只處理等待回應的狀態
    if (agent->state == AGENT_STATE_WAITING_MODBUS_RESPONSE)
    {
        handle_modbus_response(agent, current_ms);
    }
}

agent_state_t mesh_modbus_agent_get_state(const mesh_modbus_agent_t *agent)
{
    if (agent == NULL)
    {
        return AGENT_STATE_IDLE;
    }
    return agent->state;
}

bool mesh_modbus_agent_is_busy(const mesh_modbus_agent_t *agent)
{
    if (agent == NULL)
    {
        return false;
    }
    return (agent->state != AGENT_STATE_IDLE);
}

// ============================================================================
// 內部函數實作
// ============================================================================

static bool parse_rl_mode_data(mesh_modbus_agent_t *agent, const uint8_t *data, uint8_t length)
{
    // RL Mode 格式: header(2) + type(1) + MODBUS RTU package(8)
    // 最小長度 = 2 + 1 + 8 = 11
    if (length < 11)
    {
        return false;
    }

    // 檢查 header
    if ((data[0] != RL_HEADER_BYTE1) || (data[1] != RL_HEADER_BYTE2))
    {
        return false;
    }

    // 儲存 header 和 type
    agent->rl_header[0] = data[0];
    agent->rl_header[1] = data[1];
    agent->rl_type = data[2];

    // 檢查 type 是否有效
    if ((agent->rl_type != RL_TYPE_GET) &&
        (agent->rl_type != RL_TYPE_SET) &&
        (agent->rl_type != RL_TYPE_RTU))
    {
        return false;
    }

    // 提取 MODBUS RTU package (從第3個位元組開始)
    agent->modbus_request_length = length - 3;
    my_memcpy(agent->modbus_request, &data[3], agent->modbus_request_length);

    return true;
}

static bool parse_bypass_mode_data(mesh_modbus_agent_t *agent, const uint8_t *data, uint8_t length)
{
    // Bypass Mode 格式: MODBUS RTU package(8)
    // 最小長度 = 8
    if (length < 8)
    {
        return false;
    }

    // 直接複製 MODBUS RTU package
    agent->modbus_request_length = length;
    my_memcpy(agent->modbus_request, data, length);

    return true;
}

static bool send_modbus_request(mesh_modbus_agent_t *agent)
{
    // 檢查 MODBUS Client 是否忙碌
    if (modbus_rtu_client_is_busy(agent->modbus_client))
    {
        // 丟棄新收到的 mesh RTU request
        led_flash_yellow(2); // 丟棄時只閃兩次（沒有紅燈）
        return false;
    }
    led_flash_yellow(1); // 新 request 進來閃一次

    // 使用 MODBUS Client 的發送功能
    // 注意：這裡我們需要手動發送原始資料
    // 由於 modbus_rtu_client 沒有直接的 raw send API，我們需要透過其內部機制

    // 取得 MODBUS request 的資訊
    if (agent->modbus_request_length < 4)
    {
        return false;
    }

    uint8_t slave_addr = agent->modbus_request[0];
    uint8_t func_code = agent->modbus_request[1];
    uint16_t start_addr = (uint16_t)((agent->modbus_request[2] << 8) | agent->modbus_request[3]);

    // 根據 function code 決定使用哪個 API
    bool send_ok = false;

    if (func_code == MODBUS_RTU_FUNCTION_READ_HOLDING)
    {
        // Read Holding Registers (0x03)
        if (agent->modbus_request_length >= 6)
        {
            uint16_t quantity = (uint16_t)((agent->modbus_request[4] << 8) | agent->modbus_request[5]);
            send_ok = modbus_rtu_client_start_read_holding(
                agent->modbus_client,
                slave_addr,
                start_addr,
                quantity,
                agent->config.modbus_timeout_ms);
        }
    }
    else if (func_code == MODBUS_RTU_FUNCTION_READ_INPUT)
    {
        // Read Input Registers (0x04)
        if (agent->modbus_request_length >= 6)
        {
            uint16_t quantity = (uint16_t)((agent->modbus_request[4] << 8) | agent->modbus_request[5]);
            send_ok = modbus_rtu_client_start_read_input(
                agent->modbus_client,
                slave_addr,
                start_addr,
                quantity,
                agent->config.modbus_timeout_ms);
        }
    }
    else if (func_code == MODBUS_RTU_FUNCTION_WRITE_SINGLE)
    {
        // Write Single Register (0x06)
        if (agent->modbus_request_length >= 6)
        {
            uint16_t value = (uint16_t)((agent->modbus_request[4] << 8) | agent->modbus_request[5]);
            send_ok = modbus_rtu_client_start_write_single(
                agent->modbus_client,
                slave_addr,
                start_addr,
                value,
                agent->config.modbus_timeout_ms);
        }
    }
    else if (func_code == MODBUS_RTU_FUNCTION_WRITE_MULTIPLE)
    {
        // Write Multiple Registers (0x10)
        // 格式: slave(1) + func(1) + start_addr(2) + quantity(2) + byte_count(1) + data(N*2) + crc(2)
        if (agent->modbus_request_length >= 9)
        {
            uint16_t quantity = (uint16_t)((agent->modbus_request[4] << 8) | agent->modbus_request[5]);
            uint8_t byte_count = agent->modbus_request[6];

            // 轉換資料為 uint16_t 陣列
            uint16_t values[MODBUS_RTU_CLIENT_MAX_WRITE_MULTIPLE_REGISTERS];
            for (uint16_t i = 0; i < quantity && i < MODBUS_RTU_CLIENT_MAX_WRITE_MULTIPLE_REGISTERS; i++)
            {
                uint16_t offset = 7 + (i * 2);
                if (offset + 1 < agent->modbus_request_length)
                {
                    values[i] = (uint16_t)((agent->modbus_request[offset] << 8) | agent->modbus_request[offset + 1]);
                }
            }

            send_ok = modbus_rtu_client_start_write_multiple(
                agent->modbus_client,
                slave_addr,
                start_addr,
                quantity,
                values,
                agent->config.modbus_timeout_ms);
        }
    }

    if (send_ok)
    {
        led_flash_yellow(3); // 成功送出時閃三次
    }
    return send_ok;
}

static void handle_modbus_response(mesh_modbus_agent_t *agent, uint32_t current_ms)
{
    // 檢查超時
    if ((current_ms - agent->request_start_ms) > agent->config.max_response_wait_ms)
    {
        agent->state = AGENT_STATE_ERROR;
        if (agent->error_callback != NULL)
        {
            agent->error_callback(0xFF); // 超時錯誤碼
        }
        agent->state = AGENT_STATE_IDLE;
        return;
    }

    // 檢查 MODBUS Client 狀態
    modbus_rtu_client_state_t client_state = modbus_rtu_client_get_state(agent->modbus_client);

    if (client_state == MODBUS_RTU_CLIENT_STATE_COMPLETE)
    {
        // 取得原始回應資料
        uint16_t response_len = 0;
        const uint8_t *raw_response = modbus_rtu_client_get_raw_response(agent->modbus_client, &response_len);

        if ((raw_response != NULL) && (response_len > 0))
        {
            // 複製回應資料
            if (response_len > sizeof(agent->modbus_response))
            {
                response_len = sizeof(agent->modbus_response);
            }
            my_memcpy(agent->modbus_response, raw_response, response_len);
            agent->modbus_response_length = response_len;

            // 驗證 CRC（最後兩個位元組）
            bool crc_ok = false;
            if (response_len >= 4)
            {
                uint16_t received_crc = (uint16_t)((raw_response[response_len - 1] << 8) | raw_response[response_len - 2]);
                uint16_t calculated_crc = calculate_crc16(raw_response, response_len - 2);
                crc_ok = (received_crc == calculated_crc);
            }

            // 準備回傳給 Mesh 的資料
            prepare_mesh_response(agent, crc_ok);

            // 清除 MODBUS Client 狀態
            modbus_rtu_client_clear(agent->modbus_client);
        }
    }
    else if ((client_state == MODBUS_RTU_CLIENT_STATE_EXCEPTION) ||
             (client_state == MODBUS_RTU_CLIENT_STATE_ERROR) ||
             (client_state == MODBUS_RTU_CLIENT_STATE_TIMEOUT))
    {
        // MODBUS 錯誤
        agent->state = AGENT_STATE_ERROR;

        // 在 RL Mode 下回傳錯誤訊息
        if (agent->config.mode == MESH_MODBUS_AGENT_MODE_RL)
        {
            agent->mesh_tx_length = 4;
            agent->mesh_tx_buffer[0] = agent->rl_header[0];
            agent->mesh_tx_buffer[1] = agent->rl_header[1];
            agent->mesh_tx_buffer[2] = agent->rl_type;
            agent->mesh_tx_buffer[3] = ERROR_CODE_CRC_FAILED;

            if (agent->response_ready_callback != NULL)
            {
                agent->response_ready_callback(agent->mesh_tx_buffer, agent->mesh_tx_length);
            }
        }

        // 清除 MODBUS Client 狀態
        modbus_rtu_client_clear(agent->modbus_client);

        agent->state = AGENT_STATE_IDLE;
    }
}

static void prepare_mesh_response(mesh_modbus_agent_t *agent, bool crc_ok)
{
    if (!crc_ok)
    {
        // CRC 錯誤處理
        agent->state = AGENT_STATE_ERROR;
        // RL Mode: 回傳錯誤訊息
        if (agent->config.mode == MESH_MODBUS_AGENT_MODE_RL)
        {
            agent->mesh_tx_length = 4;
            agent->mesh_tx_buffer[0] = agent->rl_header[0];
            agent->mesh_tx_buffer[1] = agent->rl_header[1];
            agent->mesh_tx_buffer[2] = agent->rl_type;
            agent->mesh_tx_buffer[3] = ERROR_CODE_CRC_FAILED;
            if (agent->response_ready_callback != NULL)
            {
                agent->response_ready_callback(agent->mesh_tx_buffer, agent->mesh_tx_length);
            }
        }
        agent->state = AGENT_STATE_IDLE;
        return;
    }

    // CRC 正確：去除 CRC 後回傳
    uint16_t data_length = agent->modbus_response_length - 2; // 去除 CRC (最後2 bytes)
    if (agent->config.mode == MESH_MODBUS_AGENT_MODE_RL)
    {
        agent->mesh_tx_length = 0;
        agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->rl_header[0];
        agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->rl_header[1];
        agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->rl_type;
        for (uint16_t i = 0; i < data_length && agent->mesh_tx_length < sizeof(agent->mesh_tx_buffer); i++)
        {
            agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->modbus_response[i];
        }
    }
    else // BYPASS mode
    {
        agent->mesh_tx_length = 0;
        for (uint16_t i = 0; i < data_length && agent->mesh_tx_length < sizeof(agent->mesh_tx_buffer); i++)
        {
            agent->mesh_tx_buffer[agent->mesh_tx_length++] = agent->modbus_response[i];
        }
    }
    if (agent->response_ready_callback != NULL)
    {
        agent->response_ready_callback(agent->mesh_tx_buffer, agent->mesh_tx_length);
    }
    agent->state = AGENT_STATE_IDLE;
}

static uint16_t calculate_crc16(const uint8_t *data, uint16_t length)
{
    // 使用 MODBUS CRC 計算函數
    return modbus_crc16_compute(data, length, MODBUS_CRC_METHOD_AUTO);
}
