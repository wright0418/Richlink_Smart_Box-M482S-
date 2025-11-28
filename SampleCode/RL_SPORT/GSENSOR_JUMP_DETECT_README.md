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
# RL_SPORT — G-Sensor 跳繩計數演算法使用指南

## 概述

本模組實作基於 MXC4005XC 三軸加速度感測器的跳繩計數演算法，取代原有的 HALL Sensor 磁感應方式。透過 CMSIS DSP 函式庫進行信號處理，包含 FIR 低通濾波與峰值偵測，可自動適應不同配戴方向與跳繩特性。

主要特點：
- 使用 `#ifdef USE_GSENSOR_JUMP_DETECT` 條件編譯，可與 HALL Sensor 模式共存
- 按鈕觸發校正流程，蜂鳴器提供聲音回饋
- CMSIS DSP 加速的 FIR 低通濾波器（7 階低通，截止頻率 6Hz）
- 自動計算動態閾值，適應不同跳繩強度
- 50Hz 採樣率（Timer ISR 中斷處理）
- 防抖機制（最小間隔 200ms）

詳盡的系統使用說明（建置、參數、測試流程、除錯）請參閱本檔與專案根目錄下的 `README.md` 或 `GSENSOR_JUMP_README.md`。

## 參數位置

所有行為與門檻主要定義於：

`SampleCode/RL_SPORT/project_config.h`

請優先在該檔調整參數，包含移動偵測與跳躍偵測相關的常數（如 `MOVEMENT_*`, `JUMP_*`, `NO_MOVEMENT_TIMEOUT_*` 等）。

## 演算法要點

1. 取三軸原始值並計算向量模長（單位 g）：
   magnitude = sqrt(X^2 + Y^2 + Z^2)
2. 使用 FIR 濾波（7 taps）平滑 magnitude
3. 採用峰值偵測與閾值（threshold = baseline + multiplier * stddev）判定跳躍
4. 檢查防抖時間（`JUMP_MIN_INTERVAL_MS`）以避免重複計數
5. 校正分成靜態（baseline）與動態（stddev/peak）階段

### FIR 係數
可使用 SciPy 的 `firwin` 設計（7 taps, cutoff 6Hz, fs=50Hz）取得示例係數。

## 故障排除（精要）
- 如果無法編譯或出現 `undefined reference`：確認 `movement.c`、`ble_helpers.c`、`powerdown.c` 已加入 `VSCode/RL_SPORT.cproject.yml`。
- 如果校正失敗：確保 `Timer_Init()` 與 `GsensorInit()` 已呼叫，並避免校正過程中移動。

## 版本與作者
- v1.1 (2025-11-29) — 調整並合併 README 內容。
- 作者：Richlink 開發團隊 / AI Copilot 協助撰寫

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
    /* ... 其他欄位 */
    
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
    if (g_gsensor_sample_counter >= interval) { /* ... */ }
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
