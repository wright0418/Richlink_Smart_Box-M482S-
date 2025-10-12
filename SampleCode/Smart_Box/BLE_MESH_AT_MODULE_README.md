# BLE MESH AT 模組化重構完成

## 概述
已成功將 UART1 BLE MESH AT 命令功能重構為獨立的物件模組，提高程式碼的可維護性和重用性。

## 新增檔案

### 1. `ble_mesh_at.h` - 介面標頭檔
- **結構定義**：
  - `ble_mesh_at_config_t`：模組配置（波特率、腳位設定）
  - `ble_mesh_at_controller_t`：主控制器結構
  - `ble_mesh_at_event_t`：事件類型枚舉
  - `ble_mesh_at_state_t`：狀態枚舉

- **主要 API**：
  - `ble_mesh_at_init()`：初始化模組
  - `ble_mesh_at_update()`：更新處理（主迴圈調用）
  - `ble_mesh_at_send_ver()`：發送 AT+VER 命令
  - `ble_mesh_at_send_command()`：發送通用 AT 命令
  - 狀態查詢函數組

### 2. `ble_mesh_at.c` - 實作檔案
- **UART1 硬體控制**：時鐘、腳位、中斷設定
- **命令發送**：阻塞式字串傳送
- **CRLF 行解析**：中斷式接收與行組裝
- **事件處理**：回調機制處理各種 AT 回應
- **逾時管理**：5 秒命令逾時檢測

## 主程式改動

### `main.c` 主要變更
1. **移除舊程式碼**：
   - UART1 初始化與中斷處理
   - 行接收緩衝與解析邏輯
   - 手動命令發送與狀態管理

2. **新增模組整合**：
   - `#include "ble_mesh_at.h"`
   - 宣告全域控制器 `g_ble_at`
   - 事件回調函數 `on_ble_mesh_at_event()`
   - 在 `main()` 中初始化模組
   - 在主迴圈中調用 `ble_mesh_at_update()`

3. **簡化 UART1_IRQHandler**：
   - 只需調用 `ble_mesh_at_uart_irq_handler(&g_ble_at)`

### 項目配置更新
- **`Smart_Box.cproject.yml`**：在 User 群組中新增 `../ble_mesh_at.c`

## 使用方式

### 初始化
```c
ble_mesh_at_config_t ble_config = {
    .baudrate = 115200,
    .tx_pin_port = 0, // PA
    .tx_pin_num = 9,  // PA.9
    .rx_pin_port = 0, // PA
    .rx_pin_num = 8   // PA.8
};
ble_mesh_at_init(&g_ble_at, &ble_config, on_ble_mesh_at_event, get_system_time_ms);
```

### 事件處理
```c
void on_ble_mesh_at_event(ble_mesh_at_event_t event, const char* data)
{
    switch (event)
    {
    case BLE_MESH_AT_EVENT_VER_SUCCESS:
        // 處理版本驗證成功
        yellow_led_on();
        break;
    case BLE_MESH_AT_EVENT_LINE_RECEIVED:
        // 處理一般回應
        printf("Response: %s\n", data);
        break;
    // ... 其他事件
    }
}
```

### 命令發送
```c
// 長按按鍵觸發
if (press_duration >= 3000)
{
    if (ble_mesh_at_send_ver(&g_ble_at))
    {
        printf("AT+VER sent successfully\n");
    }
}
```

### 主迴圈更新
```c
while (1)
{
    // 其他更新...
    ble_mesh_at_update(&g_ble_at);  // 必須調用
}
```

## 技術特點

### 1. 物件導向設計
- 每個控制器實例包含完整狀態
- 支援多實例（若需要多個 UART）
- 清晰的初始化與更新生命週期

### 2. 事件驅動架構
- 非阻塞設計，不影響主迴圈
- 回調機制處理各種 AT 事件
- 分離硬體控制與應用邏輯

### 3. 錯誤處理
- 命令逾時檢測（5 秒）
- UART 錯誤旗標清除
- 緩衝區溢出保護

### 4. 彈性配置
- 可配置腳位與波特率
- 可選的事件回調
- 時間獲取函數注入

## 建置結果
- **程式大小**：Code=7156 RO-data=732 RW-data=44 ZI-data=5968
- **建置狀態**：✅ 成功
- **新增模組**：ble_mesh_at.c 已正確編譯連結

## 後續擴展
此模組化架構為後續功能提供良好基礎：
1. 添加更多 AT 命令（連接、數據傳輸等）
2. 支援命令佇列與批次處理
3. 增加重試機制與錯誤恢復
4. 擴展為完整的 BLE MESH 協議棧介面

模組化完成後，BLE MESH AT 功能更加結構化、可維護，並且容易在其他專案中重用。