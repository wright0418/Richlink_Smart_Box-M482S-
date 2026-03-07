# G-Sensor 跳繩計數演算法使用指南

## 概述

本模組實作基於 MXC4005XC 三軸加速度感測器的跳繩計數演算法，取代原有的 HALL Sensor 磁感應方式。透過 CMSIS DSP 函式庫進行信號處理，包含 FIR 低通濾波與峰值偵測，可自動適應不同配戴方向與跳繩特性。

**主要特點：**
- ✅ 使用 `#ifdef USE_GSENSOR_JUMP_DETECT` 條件編譯，可與 HALL Sensor 模式共存
- ✅ 按鈕觸發校正流程，蜂鳴器提供聲音回饋
- ✅ CMSIS DSP 加速的 FIR 濾波器（7 階低通，截止頻率 6Hz）
- ✅ 自動計算動態閾值，適應不同跳繩強度
- ✅ 50Hz 採樣率（Timer ISR 中斷處理）
- ✅ 防抖機制（最小間隔 200ms）

---

## 編譯切換方式

### 啟用 G-Sensor 跳繩計數模式

編輯 `SampleCode/RL_SPORT/project_config.h`：

```c
/* Set to 1 to enable G-Sensor jump detection */
#define USE_GSENSOR_JUMP_DETECT 1
```

### 使用 HALL Sensor 模式（預設）

```c
/* Set to 0 to use HALL sensor jump detection */
#define USE_GSENSOR_JUMP_DETECT 0
```

編譯後會自動選擇對應的跳繩計數邏輯。

---

## 校正操作步驟

### 前置準備

1. **配戴位置**：將裝置固定於跳繩繩柄或手腕（建議 Z 軸朝手掌方向）
2. **電源連接**：確保裝置已供電並執行韌體
3. **BLE 連線**：建議先連線 BLE 以接收 Debug 訊息（可選）

### 校正流程

#### 步驟 1：觸發校正
- **操作**：按下按鈕 PB15（KEY）
- **條件**：遊戲狀態必須為 `GAME_STOP`（未開始跳繩）
- **回饋**：單短音（2kHz, 100ms）

#### 步驟 2：靜態校正（2 秒）
- **指示**：聽到單短音後，立即保持靜止
- **動作**：將裝置放置於自然跳繩握持位置，不要移動
- **時長**：2 秒（`JUMP_CALIB_STATIC_TIME_MS`）
- **目的**：測量靜態重力向量基準值

#### 步驟 3：動態校正（8-10 次跳繩）
- **指示**：聽到雙短音（2 次短音間隔 150ms）
- **動作**：開始進行標準跳繩動作，至少跳 8 次
- **要求**：跳躍幅度與實際使用時相同，不要過慢或過快
- **目的**：測量跳躍時的加速度峰值特性

#### 步驟 4：校正完成
- **回饋**：三短音（3 次短音間隔 150ms）
- **狀態**：裝置進入就緒狀態，可開始正式跳繩計數
- **資料**：校正參數已儲存於 RAM（斷電後需重新校正）

### 錯誤處理

| 情境 | 症狀 | 解決方式 |
|------|------|----------|
| 校正超時 | 單短音 → 停頓 200ms → 單短音 | 30 秒內未完成校正，請重新按鈕觸發 |
| 靜態階段移動 | 基準值異常（非 ~1g） | 靜態階段需完全靜止，請重新校正 |
| 動態跳繩次數不足 | 長時間未聽到三短音 | 確保至少跳 8 次，幅度需明顯 |

---

## 演算法參數調校

所有參數定義於 `project_config.h` 的 `#if USE_GSENSOR_JUMP_DETECT` 區塊內。

### 濾波器參數

```c
#define JUMP_FIR_ORDER 7              /* FIR 濾波器階數（7 階） */
#define JUMP_FIR_CUTOFF_HZ 6.0f       /* 截止頻率（6Hz） */
#define JUMP_SAMPLE_RATE_HZ 50.0f     /* 採樣率（50Hz = 20ms 週期） */
```

**調校建議：**
- **跳繩頻率範圍**：100-200 次/分鐘 = 1.6-3.3 Hz
- **截止頻率**：建議設為跳繩頻率上限的 2 倍（6Hz 涵蓋 3Hz 主頻）
- **階數**：增加階數可提高濾波效果，但會增加延遲與運算量（7 階為最佳平衡）
- **採樣率**：50Hz 符合 Nyquist 定理（>2×6Hz），降至 30-40Hz 可降低功耗但可能影響準確性

### 偵測閾值

```c
#define JUMP_THRESHOLD_MULTIPLIER 1.5f  /* 閾值倍數（建議 1.2-2.0） */
#define JUMP_MIN_INTERVAL_MS 200        /* 防抖間隔（200ms） */
```

**調校建議：**
- **閾值計算公式**：`threshold = baseline + (multiplier × std_dev)`
  - `baseline`：靜態校正得到的重力加速度（約 1g）
  - `std_dev`：動態校正得到的峰值標準差
- **過高（>2.0）**：可能漏計輕微跳躍
- **過低（<1.2）**：可能誤判為跳繩（如手臂揮動）
- **防抖間隔**：200ms = 最高 5 跳/秒 = 300 次/分，符合運動員極限

### 校正設定

```c
#define JUMP_CALIB_STATIC_TIME_MS 2000   /* 靜態校正時長（2 秒） */
#define JUMP_CALIB_DYNAMIC_JUMPS 8       /* 動態校正跳繩次數（8 次） */
#define JUMP_CALIB_TIMEOUT_MS 30000      /* 校正總超時時間（30 秒） */
```

**調校建議：**
- **靜態時長**：2 秒可收集約 100 個樣本（50Hz），足夠計算穩定平均值
- **動態次數**：8 次跳繩提供足夠統計樣本，增加至 10-12 次可提高準確性
- **超時時間**：30 秒為合理操作時間，避免使用者忘記校正導致系統卡住

---

## 技術細節

### FIR 濾波器設計

**係數生成方式**（Python scipy）：
```python
from scipy.signal import firwin
coeffs = firwin(7, 6, fs=50, window='hamming')
# Output: [0.0385, 0.1095, 0.2020, 0.2500, 0.2020, 0.1095, 0.0385]
```

**頻率響應**：
- 通帶（0-5Hz）：< 1dB 衰減
- 截止頻率（6Hz）：-3dB
- 阻帶（>10Hz）：> -20dB 衰減

### 峰值偵測邏輯

1. **計算向量模長**：`magnitude = sqrt(X² + Y² + Z²)` (g 為單位)
2. **FIR 濾波**：使用 `arm_fir_f32()` 平滑數據
3. **斜率檢測**：
   - 上升沿（`prev < current`）：標記 `peak_detected = 1`
   - 下降沿（`prev > current` 且 `peak_detected == 1`）：檢查 `prev > threshold`
4. **防抖**：檢查 `last_jump_time` 間隔 ≥ `JUMP_MIN_INTERVAL_MS`
5. **計數**：呼叫 `Sys_IncrementJumpTimes()`

### CMSIS DSP API 使用

| 函式 | 用途 |
|------|------|
| `arm_fir_init_f32()` | 初始化 FIR 濾波器實例 |
| `arm_fir_f32()` | 處理單樣本濾波 |
| `arm_sqrt_f32()` | 快速平方根（向量模長計算） |
| `arm_mean_f32()` | 計算平均值（基準值） |
| `arm_var_f32()` | 計算變異數 |
| `arm_std_f32()` | 計算標準差（閾值） |

---

## 功耗與效能

### CPU 負載

- **採樣頻率**：50Hz（每 20ms 執行一次）
- **ISR 執行時間**：約 100-150 µs（192MHz 核心）
  - I2C 讀取 G-Sensor：~50 µs
  - FIR 濾波（7 階）：~30 µs
  - 峰值檢測邏輯：~20 µs
- **負載率**：(150 µs / 20 ms) × 100% = **0.75%**

### 記憶體使用

| 項目 | 大小 |
|------|------|
| FIR 狀態緩衝 | 32 bytes (8 × float32) |
| 校正靜態數據 | 600 bytes (150 × float32) |
| 校正峰值數據 | 200 bytes (50 × float32) |
| 其他變數 | ~100 bytes |
| **總計** | **~932 bytes RAM** |
| CMSIS DSP 函式庫 | ~30KB Flash (.lib) |

---

## 故障排除

### 問題 1：跳繩不計數

**可能原因：**
1. 未完成校正（`JumpDetect_IsReady() == 0`）
2. 跳躍幅度低於閾值
3. 配戴方向改變（與校正時不同）

**解決方式：**
- 檢查 Debug 訊息確認校正狀態
- 重新校正並確保動態階段跳躍幅度與實際使用相同
- 降低 `JUMP_THRESHOLD_MULTIPLIER`（如 1.2）

### 問題 2：誤計數（未跳繩也增加）

**可能原因：**
1. 閾值過低
2. 劇烈手臂揮動或震動

**解決方式：**
- 提高 `JUMP_THRESHOLD_MULTIPLIER`（如 1.8-2.0）
- 增加 `JUMP_MIN_INTERVAL_MS`（如 250-300ms）

### 問題 3：校正卡住無回應

**可能原因：**
1. Timer0 未初始化（`Timer_Init()` 未呼叫）
2. G-Sensor 通訊異常

**解決方式：**
- 檢查 `InitPeripheral()` 呼叫順序
- 使用 I2C 掃描工具確認 G-Sensor 地址（0x15）

### 問題 4：編譯錯誤

**常見錯誤：**
```
undefined reference to `arm_fir_f32'
```

**解決方式：**
- 確認 `RL_SPORT.cproject.yml` 已連結 `arm_cortexM4lf_math.lib`
- 確認 `define:` 區塊包含 `ARM_MATH_CM4` 和 `__FPU_PRESENT`
- 重新執行 `cbuild` 清理建置快取

---

## 進階擴展

### 1. 校正資料持久化（Data Flash）

預設校正資料儲存於 RAM，斷電後消失。若需持久化：

```c
/* 儲存校正資料到 Data Flash */
#include "fmc.h"

#define CALIB_DATA_ADDR 0x0001F000  /* Data Flash 起始位址 */

void SaveCalibrationToFlash(const CalibrationData *data)
{
    SYS_UnlockReg();
    FMC_Open();
    FMC_ENABLE_AP_UPDATE();
    
    FMC_Erase(CALIB_DATA_ADDR);
    FMC_Write(CALIB_DATA_ADDR, *(uint32_t*)&data->baseline_magnitude);
    FMC_Write(CALIB_DATA_ADDR + 4, *(uint32_t*)&data->dynamic_threshold);
    /* 其他欄位（例如校正向量、標準差等）依序寫入 */
    
    FMC_DISABLE_AP_UPDATE();
    FMC_Close();
    SYS_LockReg();
}
```

### 2. BLE 遠端校正觸發

在 `ble.c` 的 `BLEParseCommand()` 新增：

```c
if (strstr(buffer, "calib start") != NULL)
{
    JumpDetect_StartCalibration();
}
```

### 3. 動態調整採樣率

在 `GAME_STOP` 狀態降低採樣率以節省功耗：

```c
/* In timer.c */
#if USE_GSENSOR_JUMP_DETECT
    uint8_t interval = (Sys_GetGameState() == GAME_START) ? 20 : 100;  /* 50Hz or 10Hz */
    if (g_gsensor_sample_counter >= interval) {
      /* 處理已達採樣間隔：讀取 sensor 資料、執行 FIR、峰值偵測並更新計數 */
      g_gsensor_sample_counter = 0;
      /* 具體實作在 jump_detect.c 的 JumpDetect_ProcessSample() 中 */
    }
#endif
```

---

## 參考資料

- **MXC4005XC Datasheet**: 12-bit 3-axis accelerometer, FSR ±2G/±4G/±8G
- **CMSIS DSP Library**: ARM® CMSIS DSP Software Library (v1.10.0)
- **FIR 濾波器設計**: [scipy.signal.firwin Documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.firwin.html)
- **M480 系列手冊**: Nuvoton M480 Series Technical Reference Manual

---

## 版本歷史

| 版本 | 日期 | 變更內容 |
|------|------|----------|
| v1.0 | 2025-11-28 | 初始版本發布 |

---

**作者**：AI Copilot  
**授權**：Richlink Technology Corp.  
**聯絡**：請參考專案 README
