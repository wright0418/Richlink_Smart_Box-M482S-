# Modbus RTU Client 使用說明

## 簡介

`modbus_rtu_client` 是一個輕量級的 Modbus RTU Master/Client 實作，支援標準的 Modbus 功能碼，適用於嵌入式系統。

## 主要特點

- ✅ 支援標準 Modbus 功能碼（0x03, 0x04, 0x06, 0x10）
- ✅ 非阻塞式設計，不會卡住主循環
- ✅ 自動 CRC 計算與驗證
- ✅ 可配置的 timeout 機制
- ✅ 線程安全（含臨界區保護）
- ✅ 支援 UART 或其他通訊介面

## 支援的功能碼

| 功能碼 | 名稱 | 說明 |
|--------|------|------|
| 0x03 | Read Holding Registers | 讀取保持寄存器 |
| 0x04 | Read Input Registers | 讀取輸入寄存器 |
| 0x06 | Write Single Register | 寫入單一寄存器 |
| 0x10 | Write Multiple Registers | 寫入多個寄存器 |

## API 函數

### 初始化與配置

#### `modbus_rtu_client_init`
```c
bool modbus_rtu_client_init(
    modbus_rtu_client_t *client,
    const modbus_rtu_client_config_t *config
);
```
**說明**：初始化 Modbus RTU Client

**參數**：
- `client`：Client 結構體指標
- `config`：配置結構體指標

**回傳**：成功回傳 `true`，失敗回傳 `false`

#### `modbus_rtu_client_reset`
```c
void modbus_rtu_client_reset(modbus_rtu_client_t *client);
```
**說明**：重置 Client 到 IDLE 狀態

### 發送請求

#### `modbus_rtu_client_start_read_holding`
```c
bool modbus_rtu_client_start_read_holding(
    modbus_rtu_client_t *client,
    uint8_t slave_address,
    uint16_t start_address,
    uint16_t quantity,
    uint32_t timeout_ms
);
```
**說明**：讀取保持寄存器（功能碼 0x03）

**參數**：
- `slave_address`：從站地址（1-247）
- `start_address`：起始寄存器地址
- `quantity`：讀取數量（1-125）
- `timeout_ms`：超時時間（毫秒）

#### `modbus_rtu_client_start_read_input`
```c
bool modbus_rtu_client_start_read_input(
    modbus_rtu_client_t *client,
    uint8_t slave_address,
    uint16_t start_address,
    uint16_t quantity,
    uint32_t timeout_ms
);
```
**說明**：讀取輸入寄存器（功能碼 0x04）

#### `modbus_rtu_client_start_write_single`
```c
bool modbus_rtu_client_start_write_single(
    modbus_rtu_client_t *client,
    uint8_t slave_address,
    uint16_t register_address,
    uint16_t value,
    uint32_t timeout_ms
);
```
**說明**：寫入單一寄存器（功能碼 0x06）

#### `modbus_rtu_client_start_write_multiple`
```c
bool modbus_rtu_client_start_write_multiple(
    modbus_rtu_client_t *client,
    uint8_t slave_address,
    uint16_t start_address,
    uint16_t quantity,
    const uint16_t *values,
    uint32_t timeout_ms
);
```
**說明**：寫入多個寄存器（功能碼 0x10）

### 狀態查詢

#### `modbus_rtu_client_is_busy`
```c
bool modbus_rtu_client_is_busy(const modbus_rtu_client_t *client);
```
**說明**：檢查 Client 是否正在等待回應（線程安全）

**回傳**：忙碌中回傳 `true`，閒置回傳 `false`

#### `modbus_rtu_client_get_state`
```c
modbus_rtu_client_state_t modbus_rtu_client_get_state(
    const modbus_rtu_client_t *client
);
```
**說明**：獲取 Client 當前狀態（線程安全）

**回傳值**：
- `MODBUS_RTU_CLIENT_STATE_IDLE`：閒置
- `MODBUS_RTU_CLIENT_STATE_WAITING`：等待回應
- `MODBUS_RTU_CLIENT_STATE_COMPLETE`：完成
- `MODBUS_RTU_CLIENT_STATE_EXCEPTION`：Modbus 異常
- `MODBUS_RTU_CLIENT_STATE_TIMEOUT`：超時
- `MODBUS_RTU_CLIENT_STATE_ERROR`：錯誤

### 接收處理

#### `modbus_rtu_client_handle_rx_byte`
```c
void modbus_rtu_client_handle_rx_byte(
    modbus_rtu_client_t *client,
    uint8_t byte,
    uint32_t timestamp_us
);
```
**說明**：處理接收到的 UART 字節（通常在 UART RX 中斷中呼叫）

#### `modbus_rtu_client_poll`
```c
void modbus_rtu_client_poll(
    modbus_rtu_client_t *client,
    uint32_t timestamp_us
);
```
**說明**：輪詢 Client 狀態，檢查 timeout（必須定期呼叫）

### 結果讀取

#### `modbus_rtu_client_copy_response`
```c
void modbus_rtu_client_copy_response(
    const modbus_rtu_client_t *client,
    uint16_t *destination,
    uint16_t max_registers
);
```
**說明**：複製讀取到的寄存器值

#### `modbus_rtu_client_get_exception`
```c
modbus_exception_t modbus_rtu_client_get_exception(
    const modbus_rtu_client_t *client
);
```
**說明**：獲取 Modbus 異常碼

## 最小範例程式

### 範例 1：讀取保持寄存器

```c
#include "modbus_rtu_client.h"
#include "M480.h"

// 全域變數
static modbus_rtu_client_t g_client;
extern volatile uint32_t g_systick_ms;

// UART TX 回調函數
static bool uart_tx_handler(const uint8_t *data, uint16_t length, void *context)
{
    UART_T *uart = (UART_T *)context;
    
    for (uint16_t i = 0; i < length; i++)
    {
        while (UART_IS_TX_FULL(uart));
        UART_WRITE(uart, data[i]);
    }
    
    return true;
}

// Timestamp 回調函數
static uint32_t get_timestamp_us(void *context)
{
    return g_systick_ms * 1000U;
}

// UART0 中斷處理函數
void UART0_IRQHandler(void)
{
    uint32_t intsts = UART0->INTSTS;
    
    if (intsts & (UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))
    {
        while (UART_GET_RX_EMPTY(UART0) == 0)
        {
            uint8_t byte = (uint8_t)UART_READ(UART0);
            uint32_t timestamp = get_timestamp_us(NULL);
            modbus_rtu_client_handle_rx_byte(&g_client, byte, timestamp);
        }
    }
}

int main(void)
{
    // 系統初始化（省略）
    
    // 初始化 UART0
    UART_Open(UART0, 9600);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART0_IRQn);
    
    // 初始化 Modbus RTU Client
    modbus_rtu_client_config_t config = {
        .tx_handler = uart_tx_handler,
        .tx_context = UART0,
        .timestamp_callback = get_timestamp_us,
        .timestamp_context = NULL,
        .baudrate = 9600,
        .crc_method = MODBUS_CRC_METHOD_AUTO
    };
    
    if (!modbus_rtu_client_init(&g_client, &config))
    {
        // 初始化失敗
        while (1);
    }
    
    // 發送讀取請求
    uint8_t slave_addr = 1;
    uint16_t start_addr = 0x0000;
    uint16_t quantity = 10;
    uint32_t timeout_ms = 500;
    
    if (!modbus_rtu_client_start_read_holding(&g_client, slave_addr, 
                                               start_addr, quantity, timeout_ms))
    {
        // 發送失敗（Client 忙碌或參數錯誤）
    }
    
    // 主循環
    while (1)
    {
        uint32_t current_time = g_systick_ms * 1000U;
        
        // 必須定期呼叫 poll 來檢查 timeout
        modbus_rtu_client_poll(&g_client, current_time);
        
        // 檢查狀態
        modbus_rtu_client_state_t state = modbus_rtu_client_get_state(&g_client);
        
        if (state == MODBUS_RTU_CLIENT_STATE_COMPLETE)
        {
            // 讀取成功
            uint16_t values[10];
            modbus_rtu_client_copy_response(&g_client, values, 10);
            
            // 處理讀取到的數據
            for (int i = 0; i < 10; i++)
            {
                // 使用 values[i]
            }
            
            // 清除狀態，準備下一次請求
            modbus_rtu_client_clear(&g_client);
        }
        else if (state == MODBUS_RTU_CLIENT_STATE_TIMEOUT)
        {
            // 超時
            modbus_rtu_client_clear(&g_client);
        }
        else if (state == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            // Modbus 異常
            modbus_exception_t exception = modbus_rtu_client_get_exception(&g_client);
            // 處理異常
            modbus_rtu_client_clear(&g_client);
        }
        
        // 其他處理
        CLK_SysTickDelay(10000); // 延遲 10ms
    }
}
```

### 範例 2：寫入單一寄存器

```c
// 假設已初始化 g_client

void write_single_register_example(void)
{
    uint8_t slave_addr = 1;
    uint16_t register_addr = 0x0010;
    uint16_t value = 1234;
    uint32_t timeout_ms = 500;
    
    // 檢查 Client 是否空閒
    if (modbus_rtu_client_is_busy(&g_client))
    {
        // Client 忙碌中，稍後重試
        return;
    }
    
    // 發送寫入請求
    if (modbus_rtu_client_start_write_single(&g_client, slave_addr,
                                              register_addr, value, timeout_ms))
    {
        // 請求已發送，等待回應
        // 在主循環中檢查狀態
    }
}
```

### 範例 3：寫入多個寄存器

```c
void write_multiple_registers_example(void)
{
    uint8_t slave_addr = 1;
    uint16_t start_addr = 0x0020;
    uint16_t quantity = 5;
    uint16_t values[5] = {100, 200, 300, 400, 500};
    uint32_t timeout_ms = 500;
    
    if (!modbus_rtu_client_is_busy(&g_client))
    {
        modbus_rtu_client_start_write_multiple(&g_client, slave_addr,
                                                start_addr, quantity,
                                                values, timeout_ms);
    }
}
```

## 使用流程

### 完整流程圖

```
初始化
  ↓
檢查是否忙碌 (is_busy)
  ↓
發送請求 (start_read_* / start_write_*)
  ↓
在主循環中：
  - 定期呼叫 poll() 檢查 timeout
  - 檢查狀態 (get_state)
  ↓
處理結果：
  - COMPLETE：讀取數據 (copy_response)
  - TIMEOUT：處理超時
  - EXCEPTION：處理異常
  - ERROR：處理錯誤
  ↓
清除狀態 (clear)
  ↓
回到檢查是否忙碌
```

## 重要注意事項

### 1. 必須定期呼叫 `poll`
```c
// 錯誤示範：沒有呼叫 poll
while (1) {
    if (get_state() == COMPLETE) {
        // 處理結果
    }
    // ❌ 缺少 poll 呼叫，timeout 機制不會運作
}

// 正確示範
while (1) {
    modbus_rtu_client_poll(&client, get_timestamp_us(NULL));  // ✅
    if (get_state() == COMPLETE) {
        // 處理結果
    }
}
```

### 2. Timestamp 必須是微秒（us）
```c
// 如果系統時鐘是 ms，需要轉換
uint32_t timestamp_us = g_systick_ms * 1000U;
modbus_rtu_client_poll(&client, timestamp_us);
```

### 3. 線程安全
`is_busy()` 和 `get_state()` 已含臨界區保護，可在中斷和主循環中安全呼叫。

### 4. Timeout 設定建議
- **一般情況**：300-500 ms
- **慢速裝置**：500-1000 ms
- **快速輪詢**：200-300 ms

### 5. 錯誤處理
```c
modbus_rtu_client_state_t state = modbus_rtu_client_get_state(&client);

switch (state) {
    case MODBUS_RTU_CLIENT_STATE_COMPLETE:
        // 成功，讀取數據
        break;
    
    case MODBUS_RTU_CLIENT_STATE_TIMEOUT:
        // 超時，可能裝置離線或未回應
        break;
    
    case MODBUS_RTU_CLIENT_STATE_EXCEPTION:
        // Modbus 異常，檢查異常碼
        modbus_exception_t ex = modbus_rtu_client_get_exception(&client);
        break;
    
    case MODBUS_RTU_CLIENT_STATE_ERROR:
        // 通訊錯誤（CRC 錯誤、格式錯誤等）
        break;
}
```

## 常見問題

### Q1: 為什麼 request 一直被丟棄？
**A**: 可能原因：
- 沒有定期呼叫 `modbus_rtu_client_poll`，client 卡在 WAITING 狀態
- Timeout 設定太長，client 來不及回到 IDLE
- 前一個 request 還沒完成就發送新的 request

### Q2: 如何知道 request 是否成功發送？
**A**: `start_read_*` 和 `start_write_*` 函數回傳 `true` 表示成功發送到 UART，但不代表收到正確回應。需要在主循環中檢查狀態。

### Q3: 可以同時發送多個 request 嗎？
**A**: 不行。Modbus RTU 是 half-duplex，一次只能處理一個 request。必須等待前一個完成後才能發送下一個。

### Q4: 如何實現重試機制？
**A**: 
```c
uint8_t retry_count = 0;
const uint8_t max_retry = 3;

void send_with_retry(void)
{
    if (!modbus_rtu_client_is_busy(&client))
    {
        if (modbus_rtu_client_start_read_holding(&client, 1, 0, 10, 500))
        {
            retry_count = 0; // 重置計數
        }
    }
    
    // 在主循環中
    if (get_state() == TIMEOUT || get_state() == ERROR)
    {
        if (retry_count < max_retry)
        {
            retry_count++;
            modbus_rtu_client_clear(&client);
            send_with_retry(); // 重試
        }
        else
        {
            // 達到最大重試次數，放棄
        }
    }
}
```

## 版本歷史

- **v1.0** (2025-01-17)
  - 初始版本
  - 支援基本的 Modbus RTU 功能
  - 加入臨界區保護，修復競態條件問題
  - 修復 timeout 機制不運作的問題

## 授權

本代碼遵循專案授權條款。

## 技術支援

如有問題或建議，請聯繫開發團隊。
