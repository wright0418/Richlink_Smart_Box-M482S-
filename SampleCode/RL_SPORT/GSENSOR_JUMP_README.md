# RL_SPORT — G-Sensor 跳繩功能說明 (GSensor Jump Detect)

此文件說明 SampleCode/RL_SPORT 中以 G-sensor 實作跳繩（Jump Detect）的使用行為、可調參數、目錄結構與各模組責任，並提供建置、測試與參數調校建議。

## 一、專案概述

RL_SPORT 範例為 Nuvoton M480 系列開發板上的一個應用示例。本專案支援兩種跳繩偵測來源：

- G-sensor（內含跳躍檢測模組 / GSENSOR_JUMP_DETECT）
- HALL sensor（舊式方案 / 以磁感應計數）

本 README 聚焦在 G-sensor 跳繩（USE_GSENSOR_JUMP_DETECT = 1）情境下的行為、參數與驗證流程。

主要行為包括：
- 讀取 G-sensor 軸向資料，執行濾波與跳躍檢測，維護跳數計數。
- 在遊戲運作中定期透過 BLE 上報 sensor / jump count。
- 無移動偵測時 (inactivity) 觸發電源管理流程：先斷 BLE、再進入 DLPS/SPD/DPD。
- 支援校正（calibration）流程以建立 baseline / 濾波參數。


## 二、主要可調參數（位置：`SampleCode/RL_SPORT/project_config.h`）

- MOVEMENT_SAMPLE_INTERVAL_MS
  - 說明：移動偵測取樣間隔（毫秒）。預設 500 ms。
  - 影響：取樣較頻繁可更快偵測到微小移動，但會增加功耗與處理頻率。

- MOVEMENT_WINDOW_SAMPLES
  - 說明：滑動視窗大小（樣本數）。預設 8。
  - 影響：視窗越大，對短暫震動的耐受度越高；視窗越小，偵測響應越快。

- MOVEMENT_STDDEV_THRESHOLD_G
  - 說明：以 g 為單位的標準差閥值，小於此值視為「無明顯運動」。預設 0.02f。
  - 影響：閥值越低越敏感（更容易判為移動），太低會誤判雜訊為移動。

- MOVEMENT_MAG_TOLERANCE_G
  - 說明：平均 magnitude 與 1.0g 的容差（g）。預設 0.4f。
  - 影響：用來檢查平均 magnitude 是否偏離靜止 1g，較大值可容忍 sensor 偏差或校正中情況。

- NO_MOVEMENT_TIMEOUT_CONNECTED_MS / NO_MOVEMENT_TIMEOUT_DISCONNECTED_MS
  - 說明：分別為 BLE 連線與未連線情況下，達成「無移動」後等待多久進入省電流程。預設分別為 60s / 30s。

- BLE_SEND_INTERVAL_MS
  - 說明：遊戲停止（或 HALL 模式）下，BLE 上報跳數的間隔（ms）。

- JUMP 檢測相關（若 `USE_GSENSOR_JUMP_DETECT = 1`）
  - JUMP_FIR_ORDER、JUMP_FIR_CUTOFF_HZ、JUMP_SAMPLE_RATE_HZ
    - 用於 FIR 低通濾波器參數。若感測雜訊高或要抑制高頻雜訊可調低截止頻率或增加濾波器階數。
  - JUMP_THRESHOLD_MULTIPLIER
    - 跳躍判定門檻倍數（基於 baseline + multiplier * stddev）。增大可降低誤判。
  - JUMP_MIN_INTERVAL_MS
    - 跳躍之間的最小間隔，用於 debounce。
  - 校正參數：JUMP_CALIB_STATIC_TIME_MS、JUMP_CALIB_DYNAMIC_JUMPS、JUMP_CALIB_TIMEOUT_MS
    - 用於 calibration 流程，確保 baseline 與 stddev 在合適的時間視窗內收斂。


## 三、專案目錄結構（重點）

SampleCode/RL_SPORT/
- main.c                      — 程式進入、初始化、主事件迴圈
- game_logic.c / game_logic.h — 高階遊戲狀態機（開始/停止/斷線）與行為
- movement.c / movement.h     — 移動偵測模組（滑動視窗、stddev/mean 計算）
- gsensor.c / gsensor.h       — G-sensor 抽象（I2C、read axis、power down/wakeup、mag calc）
- gsensor_jump_detect.c/h     — 跳繩偵測（FIR 濾波、jump 判定、calibration）
- ble.c / ble.h               — BLE transport 與 AT 指令處理
- ble_helpers.c / ble_helpers.h — BLE 上報字串封裝（send sensor/jump）
- powerdown.c / powerdown.h  — 無動作時的斷線、DLPS、蜂鳴與進入低功耗
- led.c / led.h               — 綠燈控制（頻率/占空比）
- buzzer.c / buzzer.h         — 蜂鳴器控制
- gpio.c / gpio.h             — 板級 pin config 與按鍵處理
- timer.c / timer.h           — 毫秒 tick 與 delay
- VSCode/                     — cproject/csolution 與 build temp（CMake/Ninja）


## 四、各模組職責（簡短說明）

- `main.c`：
  - 系統初始化、Peripheral 初始化、主迴圈。
  - 單純呼叫模組 API（Game_ProcessRunning/Idle/Disconnected）與處理按鍵、BLE 收訊。

- `game_logic.c`：
  - 遊戲狀態機（GAME_START / GAME_STOP / BLE_CONNECTED / BLE_DISCONNECTED）
  - 在執行中呼叫 G-sensor 讀值並透過 `ble_helpers` 上報（或計時上報 jump count）。
  - 管理移動偵測啟停（呼叫 `Movement_*`）與決定何時觸發 powerdown（呼叫 `PowerDown_PerformSequence`）。

- `movement.c`：
  - 提供 `Movement_Init()`, `Movement_UpdateIfNeeded()`, `Movement_GetLastMovementTime()` 等 API。
  - 使用滑動視窗計算 mean / stddev，並用 `MOVEMENT_STDDEV_THRESHOLD_G` 與 `MOVEMENT_MAG_TOLERANCE_G` 判定是否為 "無移動"。

- `gsensor_jump_detect.c`：
  - 實作跳躍偵測的濾波 (FIR)、threshold 判定、debounce 與 calibration 流程。
  - 提供 `JumpDetect_StartCalibration()`, `JumpDetect_IsCalibrating()`, `JumpDetect_Process()` 等 API。

- `ble_helpers.c`：
  - 把 sensor/jump count 格式化成 BLE 要送的字串，呼叫底層 `BLESendData()`。

- `powerdown.c`：
  - 當被告知無移動且超過 timeout 時：負責關閉 LED、讓 G-sensor 進入 power-down、斷 BLE（若已連線）、觸發 DLPS，播放蜂鳴提醒，最後呼叫 `PowerMgmt_EnterSPD` 或 `PowerMgmt_EnterDPD`。

- `led.c`, `buzzer.c`, `timer.c` 等：
  - 提供硬體控制相關的抽象，避免在高階模組直接操作寄存器。


## 五、如何建置與執行（快速操作）

在 Windows PowerShell 中（本專案已提供 cbuild 與 VSCode 任務）：

- 全建（使用本專案提供的 AC6 / ARMCLANG 工具鏈）

```powershell
cbuild "d:\Nuvoton\M480_SmartBox\SampleCode\RL_SPORT\VSCode\RL_SPORT.csolution.yml" --active ARMCLANG --packs
```

- 使用 VSCode 內建的 CMSIS 任務進行 Flash / Run：
  - Run task: `CMSIS Load`（上傳映像）
  - (選擇) `CMSIS Run`（啟動 gdbserver / reset-run）

- 開發板接線/需求：請確保 CMSIS-DAP 或 pyOCD probe 連接正確、且串列線（UART0）接至主機以便看 `DBG_PRINT` 訊息。


## 六、如何測試/驗證（建議步驟）

1. 開機後觀察 `DBG_PRINT`（UART0，115200）輸出：系統啟動與裝置名稱、MAC 等資訊。
2. 按 PB15（KeyA）觸發校正流程：應看到 `Starting G-Sensor calibration...` 的訊息。
3. 進入遊戲（或讓系統模擬跳繩）並觀察：
   - `JumpDetect` 模組在每次跳躍時應更新 jump counter（可透過 BLESendData 或 DBG_PRINT 驗證）。
4. 停止動作並等待 `NO_MOVEMENT_TIMEOUT_*` 定義的時間，系統應：
   - 若 BLE 連線：先呼叫 `BLE_DISCONNECT()`，等待斷線，再進入 DLPS/SPD/DPD。若實際斷線需要較長等待，請調整 `WAIT_BLE_DISCONNECT_MS`。
   - 若 BLE 未連線：直接進入低功耗。
5. 若 calibration 流程存在問題，檢查 `gsensor_jump_detect.c` 中 calibration timeout 與 sample window 設定。


## 七、參數調校建議（實驗性）

- 若偵測到過多誤判（false positive）：
  - 增加 `MOVEMENT_STDDEV_THRESHOLD_G`（例如從 0.02 提升到 0.05）或增加 `MOVEMENT_WINDOW_SAMPLES`。
  - 在 jump 檢測中增大 `JUMP_THRESHOLD_MULTIPLIER` 或延長 `JUMP_MIN_INTERVAL_MS`。

- 若跳繩動作反應太慢或漏判：
  - 減少 `MOVEMENT_WINDOW_SAMPLES` 或減少 `MOVEMENT_SAMPLE_INTERVAL_MS`（更頻繁取樣）。
  - 在 `gsensor_jump_detect` 中降低濾波強度或減少 FIR 階數以保留更多瞬時訊號。

- 若電量太快耗盡：
  - 減少取樣頻率（MOVEMENT_SAMPLE_INTERVAL_MS 增大），並確保進入 DLPS/低功耗的 timeout 不是過長。


## 八、常見問題與除錯建議

- Linker 找不到新函式（Undefined symbol）
  - 原因：新增 .c 檔未加入 `VSCode/RL_SPORT.cproject.yml` 的 User files。
  - 解法：把檔案（movement.c、ble_helpers.c、powerdown.c）加到 cproject.yml，再重新執行 cbuild。

- `DBG_PRINT` 無輸出
  - 確認 `project_config.h` 中 `DEBUG` 定義為 1，UART0 已正確初始化，且串列線接好。

- Calibration 無法收斂或長時間無法完成
  - 檢查 `JUMP_CALIB_STATIC_TIME_MS` 與 `JUMP_CALIB_TIMEOUT_MS`，以及校正時是否有外部震動影響。


## 九、後續改進建議（roadmap）

- 將跳躍偵測的 core 算法抽成單元可測試（host 模擬），方便在 PC 上以錄製資料回放來驗證演算法。
- 對 BLE 上報增加回報/ACK 機制與錯誤重試，改善資料可靠性。
- 在 power-down 之前寫入 NVM/log，保存未上報的 jump count（若要在斷電前保存）。
- 加入 CI（至少做 build 驗證）以避免未來忘記把新檔加到 cproject.yml 導致的 link error。


---

若您願意，我可以把此 README 合併到 `SampleCode/RL_SPORT/GSENSOR_JUMP_DETECT_README.md`（若您希望覆寫現有檔）或改成 `README.md`，並且：
- 將 `ble_helpers.c` 補上 snprintf 的回傳檢查；
- 把 `project_config.h` 的 `DEBUG` 行為綁定到 cproject.yml 的 build configuration（以避免 release 中出現 printf），並提交成 git branch/PR。

要我接下來做哪一步？（建立 PR / 調整參數 / 加入 CI / 在實機上跑測試）
