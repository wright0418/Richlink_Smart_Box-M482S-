# LED 控制器測試與使用說明

## 功能概述

新的 LED 控制器系統採用物件導向設計，提供非阻塞式的 LED 控制功能。

## 主要特色

### 1. 物件導向設計
- `led_controller_t`: 單個 LED 控制器
- `led_manager_t`: LED 管理器，可管理多個 LED
- 封裝良好的 API 介面

### 2. 非阻塞式操作
- 所有 LED 控制邏輯都在 `led_manager_update_all()` 中處理
- main loop 不會被 LED 控制邏輯阻塞
- 支援高精度時間控制（毫秒級）

### 3. 靈活的控制模式
- `LED_STATE_OFF`: 關閉
- `LED_STATE_ON`: 常亮
- `LED_STATE_BLINKING`: 閃爍（可設定週期和亮燈時間）

## 硬體配置

### LED 連接腳位
- 紅色 LED: PB.1 (高電位點亮)
- 黃色 LED: PB.2 (高電位點亮)
- 藍色 LED: PB.3 (高電位點亮)

### LED 時序參數
- 紅色: 500ms週期，亮 100ms
- 黃色: 300ms週期，亮 80ms  
- 藍色: 200ms週期，亮 60ms

## API 使用範例

### 基本使用
```c
// 初始化
led_system_init();

// 設定特定 LED 狀態
led_manager_set_led_state(&led_manager, LED_RED_INDEX, LED_STATE_ON);
led_manager_set_led_state(&led_manager, LED_YELLOW_INDEX, LED_STATE_OFF);
led_manager_set_led_state(&led_manager, LED_BLUE_INDEX, LED_STATE_BLINKING);

// 在主迴圈中更新（非阻塞）
while(1) {
    led_manager_update_all(&led_manager);
    // 其他處理...
}
```

### 便利函數
```c
// 關閉所有 LED
led_manager_set_all_off(&led_manager);

// 點亮所有 LED
led_manager_set_all_on(&led_manager);

// 所有 LED 閃爍
led_manager_set_all_blinking(&led_manager);
```

## 測試功能

系統內建自動測試序列，每 10 秒執行一次：
1. 關閉所有 LED (1秒)
2. 點亮所有 LED (1秒)  
3. 恢復閃爍模式 (1秒)

## 建構與載入

```bash
# 建構專案
cbuild Smart_Box.csolution.yml --context-set --packs

# 載入到硬體
pyocd load --probe cmsisdap: --cbuild-run Smart_Box+ARMCLANG.cbuild-run.yml

# 啟動除錯
pyocd gdbserver --port 3333 --probe cmsisdap: --connect attach --persist --reset-run
```

## 串列埠輸出

系統會透過 UART0 (115200 baud) 輸出狀態資訊：
- 系統啟動訊息
- LED 模式說明
- 執行時間計數器
- 測試序列進度

## 擴展性

設計支援：
- 動態添加更多 LED
- 自定義閃爍模式
- 不同的 GPIO 埠
- 可配置的時序參數
- 支援不同的點亮邏輯（高/低電位）