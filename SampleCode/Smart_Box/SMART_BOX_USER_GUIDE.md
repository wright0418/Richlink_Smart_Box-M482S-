# Smart Box 按鍵與 LED 控制系統

## 系統概述

Smart Box 現在整合了物件導向的按鍵控制系統，支援多種按鍵事件檢測和 LED 顯示模式切換。

## 硬體配置

### LED 連接
- **紅色 LED**: PB.1 (高電位點亮)
- **黃色 LED**: PB.2 (高電位點亮)  
- **藍色 LED**: PB.3 (高電位點亮)

### 按鍵連接
- **按鍵 A**: PB.15 (低電位觸發，內建上拉電阻)

### 數位 I/O 連接
- **PA.6 (DO)**: 數位輸出腳位
- **PB.7 (DI)**: 數位輸入腳位 (內建下拉電阻)

## 按鍵功能

### 支援的按鍵事件
1. **短按**: 快速按下並釋放
2. **長按**: 持續按住 3 秒或以上  
3. **雙擊**: 1 秒內連續按兩次

### 按鍵控制功能
- **短按**: 顯示資訊訊息
- **長按 3 秒**: 循環切換 LED 顯示模式
- **雙擊**: 在正常模式與快速模式間切換

## 數位 I/O 功能

### 基本測試
- **PB.7 (DI)**: 輸入控制信號
- **PA.6 (DO)**: 輸出跟隨輸入
- **測試邏輯**: PB.7=HIGH 時 PA.6=HIGH，PB.7=LOW 時 PA.6=LOW

### 功能特色
- 即時輸入檢測和輸出控制
- 即時狀態顯示 (每 500ms)
- 簡單的邏輯跟隨測試

## LED 顯示模式

### 模式 0: Normal (正常模式)
- 紅色: 500ms 週期，100ms 亮
- 黃色: 300ms 週期，80ms 亮
- 藍色: 200ms 週期，60ms 亮

### 模式 1: Slow (慢速模式)  
- 紅色: 1000ms 週期，200ms 亮
- 黃色: 600ms 週期，160ms 亮
- 藍色: 400ms 週期，120ms 亮

### 模式 2: Fast (快速模式)
- 紅色: 250ms 週期，50ms 亮
- 黃色: 150ms 週期，40ms 亮
- 藍色: 100ms 週期，30ms 亮

### 模式 3: Wave (流水燈模式)
- 三個 LED 依序點亮，形成流水效果
- 每個 LED: 900ms 週期，300ms 亮
- 延遲: 紅色→黃色(+300ms)→藍色(+600ms)

## 技術特色

### 按鍵控制器架構
```c
// 按鍵配置
typedef struct {
    GPIO_T *port;                    // GPIO 埠
    uint32_t pin_mask;               // 腳位遮罩
    bool active_low;                 // 觸發極性
    uint32_t debounce_ms;            // 消抖時間
    uint32_t long_press_ms;          // 長按時間
    uint32_t double_click_interval_ms; // 雙擊間隔
} key_config_t;
```

### 非阻塞式設計
- 所有按鍵檢測在背景進行
- 支援即時狀態監控
- 事件驅動的回調機制

### 可擴展性
- 輕鬆添加更多按鍵
- 支援不同的按鍵配置
- 模組化的事件處理

## 使用範例

### 基本按鍵檢測
```c
// 初始化按鍵系統
key_system_init();

// 主迴圈中更新
while(1) {
    key_manager_update_all(&key_manager, on_key_event);
    // 其他處理...
}
```

### 添加新按鍵
```c
// 1. 更新按鍵數量
#define KEY_COUNT 2

// 2. 定義新按鍵索引
#define KEY_B_INDEX 1

// 3. 添加配置
key_config_t key_configs[] = {
    {PA, BIT0, true, 50, 3000, 1000}, // 按鍵 A
    {PA, BIT1, true, 50, 2000, 800},  // 按鍵 B (新增)
};
```

## 串列埠輸出

系統透過 UART0 (115200 baud) 提供詳細的狀態資訊：

### 啟動訊息
```
Smart Box with LED, Key & Digital I/O Control
Core Clock: 192000000 Hz
LED, Key & Digital I/O Controller Initialized

Key Controls:
- Short Press: Info message
- Long Press (3s): Change LED mode  
- Double Click: Toggle between Normal/Fast mode

LED Modes:
0: Normal - Different frequencies
1: Slow - Double period
2: Fast - Half period
3: Wave - Sequential lighting

Digital I/O Test:
- PB.7 (DI): Input control signal
- PA.6 (DO): Output follows input
- Logic: PB.7=HIGH -> PA.6=HIGH, PB.7=LOW -> PA.6=LOW
```

### 按鍵事件訊息
```
Key A Pressed
Long press progress: 1500 ms
Key A Long Press (3200 ms) - Changing LED mode
LED Mode changed to: 1
Key A Released (held for 3200 ms)
```

### 數位 I/O 訊息
```
Digital I/O Initialized: PA.6 (DO), PB.7 (DI)
Digital I/O Status - DI (PB.7): LOW -> DO (PA.6): LOW
Digital I/O Status - DI (PB.7): HIGH -> DO (PA.6): HIGH
Digital I/O Status - DI (PB.7): LOW -> DO (PA.6): LOW
```

## 建構與載入

```bash
# 建構專案
cbuild Smart_Box.csolution.yml --context-set --packs

# 載入到硬體  
pyocd load --probe cmsisdap: --cbuild-run Smart_Box+ARMCLANG.cbuild-run.yml

# 啟動除錯
pyocd gdbserver --port 3333 --probe cmsisdap: --connect attach --persist --reset-run
```

## 程式大小

- **程式碼**: 7948 bytes
- **唯讀資料**: 960 bytes  
- **讀寫資料**: 44 bytes
- **未初始化資料**: 5916 bytes

相比原始版本增加約 2200 bytes，主要用於按鍵控制和數位 I/O 功能。