# 數位 I/O 控制功能說明

## 功能概述

Smart Box 新增了數位 I/O 控制功能，提供簡單的數位輸入/輸出測試。

## 硬體配置

### GPIO 腳位分配
- **PB.7**: 數位輸入 (DI) - 控制信號
- **PA.6**: 數位輸出 (DO) - 被控制輸出

### 電氣特性
```
PA.6 (DO):
- 輸出模式
- 電壓範圍: 0V (LOW) / 3.3V (HIGH)
- 驅動能力: 標準 CMOS 輸出

PB.7 (DI):
- 輸入模式  
- 內建下拉電阻 (約 10kΩ)
- 輸入電壓範圍: 0V ~ 3.3V
- 邏輯閾值: 約 1.65V
```

## 測試功能

### 邏輯控制測試
1. **PB.7 輸入檢測**
   - 即時讀取輸入狀態
   - 支援 HIGH/LOW 檢測
   - 內建下拉電阻提供穩定的 LOW 狀態

2. **PA.6 輸出控制**
   - 根據 PB.7 輸入狀態控制輸出
   - PB.7 = HIGH → PA.6 = HIGH
   - PB.7 = LOW → PA.6 = LOW

3. **即時狀態顯示**
   - 每 500ms 顯示當前狀態
   - 格式: "DI (PB.7): [狀態] -> DO (PA.6): [狀態]"

### 測試方法

#### 邏輯跟隨測試
```
測試方法 1: 手動控制
PB.7 ← 手動輸入 3.3V (HIGH)  →  PA.6 應輸出 HIGH
PB.7 ← 斷開或接地 (LOW)     →  PA.6 應輸出 LOW

測試方法 2: 外部信號源
信號源 ──────── PB.7 ──────── PA.6 (輸出跟隨)
              (輸入)

期望結果:
- 當輸入 HIGH 時，輸出為 HIGH
- 當輸入 LOW 時，輸出為 LOW
- 輸出即時跟隨輸入變化
```

## 程式架構

### 初始化函數
```c
void digital_io_init(void)
{
    // PA.6 設定為輸出
    GPIO_SetMode(PA, BIT6, GPIO_MODE_OUTPUT);
    PA6 = 0; // 初始輸出低電位
    
    // PB.7 設定為輸入
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetPullCtl(PB, BIT7, GPIO_PUSEL_PULL_DOWN);
}
```

### 測試函數
```c
void digital_io_test(void)
{
    // 讀取 PB.7 輸入狀態
    bool di_state = (PB7 != 0);
    
    // 根據輸入控制輸出
    PA6 = di_state ? 1 : 0;
    
    // 顯示狀態
    printf("DI (PB.7): %s -> DO (PA.6): %s\n", 
           di_state ? "HIGH" : "LOW",
           (PA6 != 0) ? "HIGH" : "LOW");
}
```

## 串列埠輸出範例

### 初始化訊息
```
Digital I/O Initialized: PA.6 (DO), PB.7 (DI)
```

### 運行時訊息
```
Digital I/O Status - DI (PB.7): LOW -> DO (PA.6): LOW
Digital I/O Status - DI (PB.7): HIGH -> DO (PA.6): HIGH
Digital I/O Status - DI (PB.7): HIGH -> DO (PA.6): HIGH
Digital I/O Status - DI (PB.7): LOW -> DO (PA.6): LOW
```

## 技術特色

### 非阻塞式設計
- 所有 I/O 操作都在主迴圈中進行
- 不影響 LED 控制和按鍵檢測
- 即時響應輸入變化

### 實用功能
- 即時邏輯跟隨測試
- 即時狀態監控
- 清晰的串列埠回饋

### 擴展性
- 易於添加更多 I/O 腳位
- 支援不同的邏輯控制模式
- 可配置的狀態更新間隔

## 應用場景

1. **硬體除錯**: 驗證 GPIO 功能
2. **信號測試**: 測試外部設備的數位信號
3. **教學示範**: GPIO 基本操作學習
4. **原型開發**: 快速驗證數位介面設計

## 故障排除

### 常見問題
1. **PB.7 始終顯示 LOW**
   - 檢查是否正確連接到 PA.6
   - 確認外部信號電壓足夠 (>1.65V)

2. **PA.6 輸出異常**
   - 檢查 GPIO 初始化是否正確
   - 確認沒有短路或負載過重

3. **狀態更新延遲**
   - 正常現象，狀態每 500ms 更新一次
   - 實際切換每 2 秒進行一次

### 測試建議
- 使用示波器觀察 PA.6 的輸出波形
- 用三用電表測量電壓確認狀態
- 觀察串列埠輸出確認邏輯正確性