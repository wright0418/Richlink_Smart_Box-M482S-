# Mesh Modbus Agent 使用說明

## 簡介

`mesh_modbus_agent` 是一個橋接層，將 BLE Mesh 網路和 Modbus RTU 串列通訊連接起來。它接收來自 Mesh 網路的請求，轉換為 Modbus RTU 請求，並將回應轉換回 Mesh 格式。

## 主要特點

- ✅ 支援 RL Mode 和 Bypass Mode 兩種模式
- ✅ 自動處理協議轉換
- ✅ 支援所有標準 Modbus 功能碼
- ✅ 內建忙碌檢測與錯誤回報機制
- ✅ 自動 CRC 驗證
- ✅ 非阻塞式設計

## 運作模式

### RL Mode（推薦）
帶有 header 和 type 的協議格式，適用於需要區分不同類型請求的場景。

**格式**：
```
Request:  [0x82][0x76][Type][Modbus Data...]
Response: [0x82][0x76][Type][Modbus Response...]
Error:    [0x82][0x76][Type][Error Code]
```

**Type 定義**：
- `0x01`：GET（保留，未使用）
- `0x02`：SET（保留，未使用）
- `0x03`：RTU（Modbus RTU 透傳）

### Bypass Mode
直接透傳 Modbus 資料，無額外 header。

**格式**：
```
Request:  [Modbus Data...]
Response: [Modbus Response...]
Error:    [Error Code]
```

## 錯誤碼定義

| 錯誤碼 | 名稱 | 說明 |
|--------|------|------|
| 0xFD | ERROR_CODE_BUSY | 忙碌中，無法處理請求 |
| 0xFE | ERROR_CODE_CRC_FAILED | CRC 校驗失敗 |

## Agent 狀態

```c
typedef enum {
    AGENT_STATE_IDLE,                    // 閒置
    AGENT_STATE_WAITING_MODBUS_RESPONSE, // 等待 Modbus 回應
    AGENT_STATE_RESPONSE_READY,          // 回應已準備好
    AGENT_STATE_ERROR                    // 錯誤
} agent_state_t;
```

## API 函數

### 初始化

#### `mesh_modbus_agent_init`
```c
bool mesh_modbus_agent_init(
    mesh_modbus_agent_t *agent,
    const mesh_modbus_agent_config_t *config,
    modbus_rtu_client_t *modbus_client,
    void (*response_cb)(const uint8_t *data, uint8_t length),
    void (*error_cb)(uint8_t error_code),
    void (*led_flash_red_cb)(uint32_t count)
);
```
**說明**：初始化 Mesh Modbus Agent

**參數**：
- `agent`：Agent 結構體指標
- `config`：配置結構體指標
- `modbus_client`：已初始化的 Modbus RTU Client 指標
- `response_cb`：回應準備好時的回調函數
- `error_cb`：錯誤時的回調函數（可選）
- `led_flash_red_cb`：LED 指示回調（可選）

**回傳**：成功回傳 `true`，失敗回傳 `false`

### 處理資料

#### `mesh_modbus_agent_process_mesh_data`
```c
bool mesh_modbus_agent_process_mesh_data(
    mesh_modbus_agent_t *agent,
    const uint8_t *data,
    uint8_t length
);
```
**說明**：處理從 Mesh 收到的資料（已從 Hex String 轉成 bytes）

**參數**：
- `data`：資料緩衝區
- `length`：資料長度

**回傳**：
- `true`：處理成功，已發送 Modbus 請求
- `false`：處理失敗（格式錯誤或 Agent 忙碌中）

**注意**：如果回傳 `false` 且 Agent 忙碌，會立即透過 `response_cb` 回報 `ERROR_CODE_BUSY`。

### 輪詢

#### `mesh_modbus_agent_poll`
```c
void mesh_modbus_agent_poll(
    mesh_modbus_agent_t *agent,
    uint32_t current_ms
);
```
**說明**：輪詢處理 Agent 狀態（檢查 Modbus 回應），**必須在主循環中定期呼叫**

**參數**：
- `current_ms`：當前時間（毫秒）

**重要**：此函數會自動呼叫 `modbus_rtu_client_poll` 來確保 timeout 機制運作。

### 狀態查詢

#### `mesh_modbus_agent_get_state`
```c
agent_state_t mesh_modbus_agent_get_state(
    const mesh_modbus_agent_t *agent
);
```
**說明**：獲取 Agent 當前狀態

#### `mesh_modbus_agent_is_busy`
```c
bool mesh_modbus_agent_is_busy(
    const mesh_modbus_agent_t *agent
);
```
**說明**：檢查 Agent 是否忙碌中

## 最小範例程式

### 範例 1：基本 RL Mode 使用

```c
#include "mesh_modbus_agent.h"
#include "modbus_rtu_client.h"
#include "M480.h"

// 全域變數
static mesh_modbus_agent_t g_agent;
static modbus_rtu_client_t g_modbus_client;
extern volatile uint32_t g_systick_ms;

// Mesh 回應回調函數
static void agent_response_callback(const uint8_t *data, uint8_t length)
{
    // 將回應轉換為 Hex String 並透過 BLE Mesh 發送給主機
    // 例如：將 data 轉換為 "82760301AB..." 格式
    char hex_string[80];
    for (uint8_t i = 0; i < length; i++)
    {
        sprintf(&hex_string[i * 2], "%02X", data[i]);
    }
    
    // 透過 Mesh 發送 hex_string
    // send_mesh_response(hex_string);
}

// 錯誤回調函數
static void agent_error_callback(uint8_t error_code)
{
    // 處理錯誤
    if (error_code == 0xFF)
    {
        // Timeout 錯誤
    }
}

int main(void)
{
    // 系統初始化（省略）
    
    // 1. 初始化 Modbus RTU Client（參考 MODBUS_RTU_CLIENT_USAGE.md）
    modbus_rtu_client_config_t modbus_config = {
        .tx_handler = uart_tx_handler,
        .tx_context = UART0,
        .timestamp_callback = get_timestamp_us,
        .timestamp_context = NULL,
        .baudrate = 9600,
        .crc_method = MODBUS_CRC_METHOD_AUTO
    };
    modbus_rtu_client_init(&g_modbus_client, &modbus_config);
    
    // 2. 初始化 Mesh Modbus Agent
    mesh_modbus_agent_config_t agent_config = {
        .mode = MESH_MODBUS_AGENT_MODE_RL,  // 使用 RL Mode
        .modbus_timeout_ms = 300,            // Modbus timeout 300ms
        .max_response_wait_ms = 500          // 最大等待 500ms
    };
    
    mesh_modbus_agent_init(&g_agent, &agent_config, &g_modbus_client,
                          agent_response_callback,
                          agent_error_callback,
                          NULL);
    
    // 主循環
    while (1)
    {
        uint32_t current_time = g_systick_ms;
        
        // 必須定期呼叫 poll
        mesh_modbus_agent_poll(&g_agent, current_time);
        
        // 處理 Mesh 接收（此處省略 BLE Mesh 接收邏輯）
        // 當收到 Mesh 資料時，呼叫 process_mesh_data
        
        CLK_SysTickDelay(10000); // 延遲 10ms
    }
}
```

### 範例 2：處理 Mesh 請求

```c
// 當從 BLE Mesh 收到資料時呼叫此函數
void on_mesh_data_received(const char *hex_string)
{
    // 假設收到的是 Hex String，例如："8276030101000001"
    // 需要先轉換為 bytes
    
    uint8_t data[40];
    uint8_t length = 0;
    
    // 將 Hex String 轉換為 bytes
    for (size_t i = 0; i < strlen(hex_string) && i < sizeof(data) * 2; i += 2)
    {
        char byte_str[3] = {hex_string[i], hex_string[i + 1], '\0'};
        data[length++] = (uint8_t)strtol(byte_str, NULL, 16);
    }
    
    // 處理資料
    if (mesh_modbus_agent_process_mesh_data(&g_agent, data, length))
    {
        // 成功，Modbus 請求已發送
        // 等待 response_callback 被呼叫
    }
    else
    {
        // 失敗（可能 Agent 忙碌或格式錯誤）
        // 如果是忙碌，response_callback 會收到 ERROR_CODE_BUSY
    }
}
```

### 範例 3：RL Mode 協議範例

#### 讀取保持寄存器
```
Mesh Request (Hex String):
"8276 03 01 0003 000A"
 ││││ ││ ││ ││││ ││││
 ││││ ││ ││ ││││ └└└└─ Quantity: 10 個寄存器
 ││││ ││ ││ └└└└───── Start Address: 0x0003
 ││││ ││ └└────────── Function Code: 0x03 (Read Holding)
 ││││ ││             Slave Address: 0x01
 ││││ └└────────────── Type: 0x03 (RTU)
 └└└└───────────────── Header: 0x82 0x76

Modbus RTU Request (實際發送到 UART):
01 03 0003 000A [CRC16]

Modbus RTU Response (從 UART 接收):
01 03 14 [20 bytes data] [CRC16]

Mesh Response (Hex String 回傳給主機):
"8276 03 01 03 14 [data...] [CRC]"
 ││││ ││ ││││││││││││││││││││││││
 ││││ ││ └└└└└└└└└└└└└└└└└└└└└─ Modbus response 包含 CRC
 ││││ └└──────────────────────── Type: 0x03
 └└└└────────────────────────── Header: 0x82 0x76
```

#### 寫入單一寄存器
```
Mesh Request:
"8276 03 01 06 0010 04D2"
      └─ Type 0x03 (RTU)
         └─ Slave 0x01
            └─ Function 0x06 (Write Single)
               └─ Register 0x0010
                  └─ Value 0x04D2 (1234)

Mesh Response:
"8276 03 01 06 0010 04D2 [CRC]"
(Echo back with CRC)
```

#### 忙碌錯誤
```
Mesh Request:
"8276 03 01 0003 000A"

Mesh Response (如果 Agent 忙碌):
"8276 03 FD"
      └─ Type 0x03
         └─ Error Code 0xFD (BUSY)
```

### 範例 4：Bypass Mode 使用

```c
// 初始化為 Bypass Mode
mesh_modbus_agent_config_t config = {
    .mode = MESH_MODBUS_AGENT_MODE_BYPASS,  // Bypass Mode
    .modbus_timeout_ms = 300,
    .max_response_wait_ms = 500
};

mesh_modbus_agent_init(&g_agent, &config, &g_modbus_client,
                      response_callback, error_callback, NULL);

// Bypass Mode 資料格式範例
// Mesh Request: "01 03 0003 000A" (直接是 Modbus RTU 資料，無 header)
// Mesh Response: "01 03 14 [data...] [CRC]" (直接是 Modbus RTU 回應)
// Error Response: "FD" (只有錯誤碼)
```

## 使用流程圖

```
初始化 Agent
  ↓
在主循環中定期呼叫 poll()
  ↓
收到 Mesh 資料
  ↓
呼叫 process_mesh_data()
  ↓
Agent 檢查是否忙碌
  │
  ├─ 忙碌 → 立即回應 0xFD → response_callback
  │
  └─ 閒置 → 解析 Mesh 資料 → 發送 Modbus request
              ↓
         等待 Modbus 回應
              ↓
         ┌─────┴─────┐
         ↓           ↓
      成功回應    超時/錯誤
         ↓           ↓
    組裝 Mesh    回報錯誤
      回應        (error_callback)
         ↓
   response_callback
         ↓
      完成
```

## 重要注意事項

### 1. 必須定期呼叫 `poll`
```c
// 正確示範
while (1) {
    mesh_modbus_agent_poll(&agent, g_systick_ms);  // ✅ 定期呼叫
    // 其他處理
}

// 錯誤示範
while (1) {
    // ❌ 沒有呼叫 poll，timeout 機制不會運作
    // Agent 會卡住
}
```

### 2. Mesh 資料必須先轉換為 bytes
```c
// 錯誤：直接傳 Hex String
mesh_modbus_agent_process_mesh_data(&agent, "8276030101", 10);  // ❌

// 正確：先轉換為 bytes
uint8_t data[] = {0x82, 0x76, 0x03, 0x01, 0x01};
mesh_modbus_agent_process_mesh_data(&agent, data, 5);  // ✅
```

### 3. 回調函數會在不同時機被呼叫
```c
// response_callback 會在以下時機被呼叫：
// 1. Modbus 回應成功
// 2. CRC 校驗失敗
// 3. Agent 忙碌（立即回應 0xFD）

// error_callback 會在以下時機被呼叫：
// 1. Timeout
// 2. Modbus 通訊錯誤
```

### 4. Timeout 設定建議
```c
mesh_modbus_agent_config_t config = {
    .modbus_timeout_ms = 300,        // Modbus RTU 層 timeout
    .max_response_wait_ms = 500      // Agent 層 timeout（應大於 modbus_timeout）
};
```

### 5. 錯誤處理流程
```c
static void response_callback(const uint8_t *data, uint8_t length)
{
    // 檢查是否為錯誤回應
    if (length == 4 && data[3] == 0xFD)
    {
        // 忙碌錯誤，主機應稍後重試
        return;
    }
    
    if (length == 4 && data[3] == 0xFE)
    {
        // CRC 錯誤
        return;
    }
    
    // 正常回應，處理資料
    // ...
}
```

## 完整範例：整合 BLE Mesh

```c
#include "mesh_modbus_agent.h"
#include "ble_mesh_at.h"

// 全域變數
static mesh_modbus_agent_t g_agent;
static ble_mesh_at_controller_t g_ble_controller;

// BLE Mesh 接收回調
static void on_ble_mesh_received(const char *hex_data)
{
    // 轉換 Hex String 為 bytes
    uint8_t data[40];
    uint8_t length = hex_string_to_bytes(hex_data, data, sizeof(data));
    
    // 處理資料
    mesh_modbus_agent_process_mesh_data(&g_agent, data, length);
}

// Agent 回應回調
static void agent_response_callback(const uint8_t *data, uint8_t length)
{
    // 轉換 bytes 為 Hex String
    char hex_string[80];
    bytes_to_hex_string(data, length, hex_string, sizeof(hex_string));
    
    // 透過 BLE Mesh 發送回應
    ble_mesh_at_send_data(&g_ble_controller, hex_string);
}

int main(void)
{
    // 初始化 BLE Mesh
    ble_mesh_at_init(&g_ble_controller, ...);
    ble_mesh_at_set_data_callback(&g_ble_controller, on_ble_mesh_received);
    
    // 初始化 Modbus RTU Client
    modbus_rtu_client_init(&g_modbus_client, &modbus_config);
    
    // 初始化 Agent
    mesh_modbus_agent_config_t agent_config = {
        .mode = MESH_MODBUS_AGENT_MODE_RL,
        .modbus_timeout_ms = 300,
        .max_response_wait_ms = 500
    };
    mesh_modbus_agent_init(&g_agent, &agent_config, &g_modbus_client,
                          agent_response_callback, NULL, NULL);
    
    // 主循環
    while (1)
    {
        uint32_t current_time = g_systick_ms;
        
        // 定期呼叫 poll
        mesh_modbus_agent_poll(&g_agent, current_time);
        ble_mesh_at_poll(&g_ble_controller, current_time);
        
        CLK_SysTickDelay(10000);
    }
}
```

## 常見問題

### Q1: 為什麼主機重送 request 時還是收到 0xFD？
**A**: 可能原因：
- `mesh_modbus_agent_poll` 沒有被定期呼叫
- `modbus_rtu_client_poll` 沒有被呼叫（已在 `mesh_modbus_agent_poll` 中自動呼叫）
- Timeout 設定太長，Agent 來不及回到 IDLE

**解決方法**：確保在主循環中定期呼叫 `mesh_modbus_agent_poll`。

### Q2: 如何區分不同的錯誤？
**A**: 
- `0xFD` (BUSY)：透過 `response_callback` 回傳
- `0xFE` (CRC_FAILED)：透過 `response_callback` 回傳
- `0xFF` (TIMEOUT)：透過 `error_callback` 回傳（如果有設定）

### Q3: RL Mode 和 Bypass Mode 如何選擇？
**A**:
- **RL Mode**：推薦使用，可以區分不同類型的請求，有完整的 header
- **Bypass Mode**：簡單透傳，適用於只需要 Modbus RTU 功能的場景

### Q4: 如何實現主機端的重試機制？
**A**: 主機端收到 `0xFD` 錯誤碼時：
```python
# Python 主機端範例
def send_with_retry(request_data, max_retry=3):
    for i in range(max_retry):
        response = send_mesh_request(request_data)
        
        if response[-1] == 0xFD:  # 忙碌錯誤
            time.sleep(0.1)  # 等待 100ms
            continue
        
        return response  # 成功或其他錯誤
    
    raise Exception("Max retry exceeded")
```

### Q5: 如何測試 Agent 是否正常運作？
**A**: 使用 LED 指示：
- 黃燈閃 1 下：新 request 正常處理
- 黃燈閃 2 下：request 被丟棄（忙碌）
- 黃燈閃 2 下 + 紅燈 1 秒：RTU 通訊異常

## 版本歷史

- **v1.0** (2025-01-17)
  - 初始版本
  - 支援 RL Mode 和 Bypass Mode
  - 加入忙碌檢測與錯誤回報機制 (0xFD)
  - 修復 poll 未呼叫 modbus_rtu_client_poll 導致狀態機卡住的問題

## 授權

本代碼遵循專案授權條款。

## 技術支援

如有問題或建議，請聯繫開發團隊。
