# RL_SPORT (Richlink) — 專案說明與快速使用指南

此 README 聚焦於 SampleCode/RL_SPORT 的整體使用、建置、參數說明與各模組責任，並將 G-Sensor 跳繩演算法細節放在 `GSENSOR_JUMP_DETECT_README.md` 中以供深入閱讀。

## 目標與摘要

RL_SPORT 為 Nuvoton M480 系列開發板之範例專案，提供跳繩計數等應用演示。主要特色：

- 支援 G-sensor（MXC400 系列）跳繩偵測（濾波 + 峰值偵測 + 校正）
- 支援 HALL sensor 模式（可透過編譯開關切換）
- BLE 上報機制、LED 與蜂鳴回饋、以及無動作時的省電流程

如需 G-sensor 演算法細節（FIR 係數、校正流程、參數調校等），請參閱 `GSENSOR_JUMP_DETECT_README.md`。

## 快速開始（Build & Flash）

1. 在 Windows PowerShell 中建置（使用專案提供的 cbuild 與 AC6/ARMCLANG）:

```powershell
cbuild "d:\Nuvoton\M480_SmartBox\SampleCode\RL_SPORT\VSCode\RL_SPORT.csolution.yml" --active ARMCLANG --packs
```

2. 使用 VSCode 的 CMSIS 任務或 pyOCD 進行上傳（Load）與執行（Run）：
   - Run task: `CMSIS Load`（上傳映像）
   - Run task: `CMSIS Run`（啟動 gdbserver，reset-run）

3. 開發板串列線（UART0, 115200）可用於接收 `DBG_PRINT` 訊息以便偵錯。

## 編譯選項（切換偵測模式）

在 `SampleCode/RL_SPORT/project_config.h`：

```c
/* Set to 1 to enable G-Sensor jump detection */
#define USE_GSENSOR_JUMP_DETECT 1
```

設為 0 則使用 HALL sensor 之偵測流程。

## 重要檔案與模組（目錄結構）

SampleCode/RL_SPORT/
- `main.c` — 系統啟動、初始化、主事件迴圈
- `game_logic.c/.h` — 高階狀態機 (GAME_START/GAME_STOP) 與行為
- `movement.c/.h` — 移動偵測（滑動視窗、stddev/mean 計算）
- `gsensor.c/.h` — G-sensor 抽象層（I2C、讀值、power management）
- `gsensor_jump_detect.c/.h` — 跳繩偵測（FIR 濾波、峰值檢測、校正）
- `ble.c/.h`, `ble_helpers.c/.h` — BLE 傳輸與上報封裝
- `powerdown.c/.h` — 省電/斷線流程（蜂鳴、DLPS、SPD/DPD）
- `led.c/.h`, `buzzer.c/.h` — 外設控制
- `timer.c/.h` — 毫秒 tick 與 delay
- `VSCode/` — 專案的 cproject/csolution 與建置暫存

## 主要可調參數（位置：`project_config.h`）

- MOVEMENT_SAMPLE_INTERVAL_MS（預設 500 ms）
- MOVEMENT_WINDOW_SAMPLES（預設 8）
- MOVEMENT_STDDEV_THRESHOLD_G（預設 0.02f）
- MOVEMENT_MAG_TOLERANCE_G（預設 0.4f）
- NO_MOVEMENT_TIMEOUT_CONNECTED_MS（預設 60s）
- NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS（預設 30s）
- BLE_SEND_INTERVAL_MS
- JUMP_*（FIR_ORDER, CUTOFF_HZ, SAMPLE_RATE_HZ, THRESHOLD_MULTIPLIER, MIN_INTERVAL_MS, CALIB_*）

調整這些常數會影響偵測靈敏度、功耗與使用者經驗。

## 運作流程簡述

1. 開機 → 各模組初始化（Timer, I2C, Gsensor, BLE, LED, Buzzer）
2. 主迴圈：處理按鍵、BLE 收訊、呼叫 `Game_Process*()` 進行遊戲邏輯
3. 若啟用 G-sensor 模式，JumpDetect 模組以 50Hz 採樣並計算是否有跳躍，更新 jump counter
4. 若長時間偵測到無移動（由 `movement.c` 判定），觸發 `PowerDown_PerformSequence()` 進入低功耗

## 測試與驗證步驟（建議）

- 使用串列監控 (UART0, 115200) 觀察 `DBG_PRINT`，確認系統啟動與狀態
- 按 PB15（KeyA）觸發校正，跟隨 README 中的校正步驟完成靜態與動態校正
- 進行跳繩動作，觀察 jump counter（DBG_PRINT 或 BLE 上報）是否符合預期
- 停止動作並等待 NO_MOVEMENT_TIMEOUT 觀察省電行為

## 常見問題快速排除

- Undefined reference：確認新增 .c 檔已加入 `VSCode/RL_SPORT.cproject.yml`
- DBG_PRINT 無輸出：檢查 `project_config.h` 的 DEBUG 定義和 UART 初始化
- 校正失敗：確認 Timer 與 G-Sensor 初始化，並重試校正流程

## 深入閱讀

- G-sensor 跳繩演算法詳述請參閱：`GSENSOR_JUMP_DETECT_README.md`

---

若您要我：
- 將本 README 合併提交為 PR，或
- 調整 `ble_helpers.c` 的 snprintf 安全處理，或
- 把 DEBUG 的控制綁定到 cproject.yml 的 build configuration（Release/Debug），

請選一項，我會繼續實作並在本 repo 提交變更。