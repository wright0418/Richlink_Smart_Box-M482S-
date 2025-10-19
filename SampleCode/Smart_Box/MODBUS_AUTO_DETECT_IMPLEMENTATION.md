# MODBUS RTU 自動偵測功能實作總結

## 📅 實作日期
2025-10-19

## 🎯 功能概述
開機時自動掃描 Modbus RTU 設備，測試不同的 Baudrate 和 Device ID 組合，找到連接的設備。如果找不到設備則使用預設值。

## 📋 實作內容

### 1. 新增檔案

#### `modbus_auto_detect.h`
自動偵測模組標頭檔，定義：
- `modbus_detect_result_t` - 偵測結果結構
- `modbus_detect_config_t` - 偵測配置結構
- `modbus_auto_detect_scan()` - 主要掃描函數
- `modbus_auto_detect_get_default_config()` - 取得預設配置

#### `modbus_auto_detect.c`
自動偵測模組實作，包含：
- UART 動態配置功能
- Baudrate 掃描順序：9600 → 38400 → 4800 → 57600 → 115200
- Device ID 掃描範圍：1-10（快速模式）
- 每個設備測試超時：80ms
- 使用 Function Code 0x03 (Read Holding Registers) 測試

### 2. 修改現有檔案

#### `modbus_sensor_manager.h`
新增函數聲明：
- `modbus_sensor_manager_set_baudrate()` - 動態設定 baudrate
- `modbus_sensor_manager_set_slave_address()` - 動態設定 slave address

#### `modbus_sensor_manager.c`
實作動態配置函數：
- 使用 `uart_rs485_driver_set_baudrate()` 重新配置 UART
- 更新 Modbus client 配置
- 重置 client 狀態

#### `led_indicator.h` 和 `led_indicator.c`
新增紅燈閃爍功能：
- 新增 `led_flash_red()` 函數
- 新增紅燈閃爍狀態機
- 定義 `RED_FLASH_ON_MS` 和 `RED_FLASH_OFF_MS` 常數

#### `main.c`
整合自動偵測流程：
- 在 `SysTick_Config()` 後執行自動偵測
- 根據偵測結果設定 LED 指示：
  - **找到設備**：不閃燈（正常運行）
  - **未找到設備**：紅燈閃 3 次（使用預設值 9600, ID=1）
  - **掃描失敗**：紅燈閃 5 次（使用預設值）
- 使用偵測到的參數初始化 Modbus Sensor Manager
- 新增 `get_system_time_ms_cb()` 回調包裝函數

#### `Smart_Box.cproject.yml`
更新編譯配置：
- 在 User 群組中新增 `../modbus_auto_detect.c`

## 🔧 技術細節

### 掃描策略
- **快速模式**（預設）：掃描 Device ID 1-10
- **Baudrate 優先順序**：9600（最常用）→ 38400 → 4800 → 57600 → 115200
- **測試方法**：Read Holding Register (FC 0x03)，地址 0x0000，數量 1
- **判定標準**：正常回應或異常回應都視為找到設備，只有超時才視為不存在

### 時間估算
- 每個 Device ID 測試：80ms
- 快速模式總時間：10 個 ID × 5 個 baudrate × 80ms ≈ **4 秒**
- 最佳情況（第一個就中）：**80ms**

### LED 指示規則
| 結果 | LED 行為 | 說明 |
|------|---------|------|
| 找到設備 | 不閃燈 | 使用偵測到的參數正常運行 |
| 未找到設備 | 紅燈閃 3 次 | 使用預設值 (9600, ID=1) |
| 掃描失敗 | 紅燈閃 5 次 | 硬體錯誤，使用預設值 |

## 📝 使用方式

### 正常開機流程
1. 系統初始化（時鐘、GPIO、SysTick）
2. **執行 Modbus RTU 自動偵測**（約 4 秒）
3. 根據偵測結果初始化 Modbus Sensor Manager
4. 初始化 BLE Mesh 和其他模組
5. 進入主迴圈

### 預設配置
```c
// 快速掃描模式
quick_scan = true              // 只掃描 ID 1-10
per_device_timeout_ms = 80     // 每個設備 80ms 超時
test_register_address = 0x0000 // 測試地址
```

### 自訂配置範例
```c
modbus_detect_config_t detect_config = {
    .quick_scan = false,           // 完整掃描 (1-255)
    .per_device_timeout_ms = 100,  // 增加超時時間
    .test_register_address = 0x0000,
    .max_device_id = 50            // 只掃描到 ID 50
};
```

## ⚠️ 注意事項

1. **掃描時間**：快速模式約 4 秒，完整掃描會超過 2 分鐘
2. **硬體需求**：需要正確配置 RS485 方向控制（PB14）
3. **UART 配置**：掃描過程中會暫時修改 UART0 設定
4. **設備回應**：某些設備可能對地址 0x0000 不回應，可能需要調整測試地址
5. **幹擾影響**：RS485 總線雜訊可能導致誤判，建議在乾淨環境下掃描

## 🔍 除錯建議

如果自動偵測失敗：
1. 檢查 RS485 硬體連接（A/B 線、方向控制）
2. 確認 Modbus 設備已上電且正常運作
3. 嘗試手動設定已知的 baudrate 和 device ID 測試
4. 使用 Modbus 測試工具驗證設備回應
5. 檢查是否需要調整 `test_register_address`

## 📊 成果

### ✅ 已完成
- [x] 建立 `modbus_auto_detect.h` 標頭檔
- [x] 建立 `modbus_auto_detect.c` 實作檔
- [x] 修改 `modbus_sensor_manager` 支援動態配置
- [x] 新增紅燈閃爍功能
- [x] 整合到 `main.c`
- [x] 更新編譯配置 (`cproject.yml`)

### 🎉 功能特點
- ✨ 智能掃描：優先常用 baudrate 和 Device ID
- ✨ 快速模式：4 秒完成掃描（1-10）
- ✨ LED 指示：清楚顯示偵測結果
- ✨ 容錯設計：偵測失敗仍可使用預設值運行
- ✨ 非阻塞：掃描期間可顯示 LED 狀態

## 🚀 後續建議

### 可選增強功能
1. **Data Flash 儲存**：記住上次成功的參數，下次優先嘗試
2. **進度指示**：掃描過程中藍燈閃爍顯示進度
3. **手動觸發**：透過按鍵或命令重新掃描
4. **詳細日誌**：透過 UART 輸出掃描詳細資訊
5. **智能重試**：通訊失敗時自動重新掃描

---

**實作狀態**：✅ 全部完成  
**測試狀態**：⏳ 待測試  
**文件狀態**：✅ 已完成
