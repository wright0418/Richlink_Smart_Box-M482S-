# RL_SPORT_FW_規格

## 目的
此文件為 `RL_SPORT` 韌體之流程與行為規格（中文，繁體），供團隊後續維護、測試與作為新專案撰寫範例。內容來源為 `SampleCode/RL_SPORT` 程式碼檢視（主要檔案：`main.c`, `ble.c`, `gsensor.c`, `game_logic.c`, `power_mgmt.c`, `gpio.c`, `i2c.c`, `led.c`, `timer.c`, `system_status.c`, `project_config.h`）。

## 總覽架構
- 系統核心與板級初始化：`main.c`（`RL_InitSystemCore`, `RL_InitBoardInputs`, `RL_InitDrivers`, `RL_InitApplication`）
- 周邊驅動：`i2c.c`, `gsensor.c`, `led.c`, `timer.c`, `adc`（電池量測）
- 應用邏輯：`game_logic.c`（運動/靜止偵測、上報機制）
- 通訊：`ble.c`（UART1 作為 BLE transport，命令解析與 AT 流程）
- 電源管理：`power_mgmt.c`（SPD/DPD 模式、USB 偵測、PowerLock）
- 板級 I/O 與中斷：`gpio.c`（按鍵 PB15、HALL PB7/PB8、PC5 G-sensor INT）
- 全域狀態：`system_status.c`（`g_sys`）

## 啟動流程（Boot）
1. Reset → 進入 `main()`。
2. 呼叫 `RL_InitSystemCore()`：
   - 解鎖受保護暫存器、時脈（HXT, PLL → HCLK 設定）、PCLK 設定
   - `Board_ReleaseIOPD()` 釋放 I/O hold
   - 呼叫 `PowerMgmt_DetectUsbCharge()` 檢查 USB 充電模式
  - 若為 USB 充電模式則在主迴圈以 `PowerMgmt_ChargeModeInit()` + `PowerMgmt_ChargeModeProcess()` 進行充電流程（非阻塞狀態式處理）
3. `RL_InitBoardInputs()`：設定按鍵/中斷腳位（`Gpio_Init()`）
4. `RL_InitDrivers()`：
   - `Timer_Init()`（1ms tick）
   - `Gsensor_Init()`（I2C 打開、設定 FSR）
   - `Adc_InitBattery()`（電池 ADC）
   - `UART_Open(UART0,115200)`（Debug）
   - `Ble_Init(115200)`（UART1）
   - `Led_Init()`, `Buzzer_Init()`
5. `RL_InitApplication()`：
   - `Sys_Init()`, `Game_Init()`，若啟用 Jump Detect 則啟動校正流程
6. 執行 `BoardTest_RunAll()`（啟動時一次性板測）
7. 進入主迴圈

## 主迴圈行為（high-level）
每回合主迴圈（`while(1)`）:
- 處理測試模式（`TestMode_PollEnter()` / `TestMode_RunMenuIfActive()`）
- 若 `UsbHidMouse_TestIsActive()`：以 1ms 更新 USB 測試循環，跳過一般流程
- G-sensor 自動校正週期與跳躍偵測（若啟用 `USE_GSENSOR_JUMP_DETECT`）
- 定期讀取 G-sensor、印出 debug（200ms）
- 每秒檢查電池電壓（`LOW_BATT_CHECK_INTERVAL_MS`）並設定低電旗標
- 處理 HALL 中斷旗標記錄
- 處理按鍵事件（`Sys_GetKeyAFlag()`）→ `ProcessKeyAEvent()` → `Game_ResetMovementTimer()`
- 處理 BLE 消息/遊戲狀態機（`RL_HandleBleAndGameState()`）
- 處理 idle 超時自動關機（`RL_HandleIdlePowerOff()`）
- 更新 LED 狀態（`RL_UpdateLedState()`）

## 狀態機與行為細節
- BLE 狀態（`g_sys.ble_state`）：`BLE_CONNECTED` / `BLE_DISCONNECTED`。由 `ble.c` 的 UART 接收回應解析決定。
- 遊戲狀態（`g_sys.game_state`）：`GAME_START` / `GAME_STOP`。由 BLE 指令（例如 `get cycle` 觸發 start，`set end` 觸發 stop）或本機事件變更。
- 運動/閒置偵測（`game_logic.c`）：
  - 以 sliding window 計算 magnitude 的 mean 與 stddev（參數在 `project_config.h`），判定「無動作」
  - 若無動作且超時（聯線/未聯線分別不同 timeout），則設定 `idle_state` -> 觸發 `RL_HandleIdlePowerOff()`
- 跳繩計數：兩種模式擇一
  - HALL 模式（`USE_GSENSOR_JUMP_DETECT == 0`）：PB7 中斷僅累積 edge 事件；主迴圈以 2 edge = 1 jump 規則換算跳數（ISR 減負）
  - G-sensor 模式（`USE_GSENSOR_JUMP_DETECT == 1`）：在 `gsensor_jump_detect` 模組執行濾波/閾值/校正並在主迴圈或模組內增加跳數

## BLE 行為 / AT 流程（`ble.c`）
- 使用 UART1 作為 BLE module transport，ISR 收到一整行後透過 `BLEParseCommand()` 判斷
- 支援命令示例：
  - 連線/斷線通知（改變 `g_sys.ble_state`）
  - `get cycle` → 進入 `GAME_START` 並回應、蜂鳴提示
  - `set end` → `GAME_STOP`、重置計數並發出完成提示
  - `MAC_ADDR` / `DEVICE_NAME` → 解析並存入 `g_sys`
- BLE 模組啟動流程包含 rename（`Ble_RenameFlow`）以將裝置名稱改為 `ROPE_XXXX`（取 MAC 後 4 碼）
- BLE 模組啟動流程改為非阻塞 rename 狀態機（`Ble_RenameFlowStart()` / `Ble_RenameFlowProcess()` / `Ble_RenameFlowIsDone()`）以避免開機卡住

## LED / Buzzer 行為
- `SetGreenLedMode(freq, duty)` 控制綠燈閃爍模式（模組化，LED 時序由 `timer.c` 每 1ms callback 更新）
- LED 模式優先級（簡述）:
  - 低電模式：高頻閃爍（`LOW_BATT_LED_FREQ_HZ`）
  - 遊戲中 (`GAME_START`)：快速短亮
  - BLE 已連線但遊戲未開始：慢閃
  - G-sensor 校正過程：特定校正閃爍
- Buzzer 用於指示開始/結束或錯誤（短促蜂鳴序列），透過 `BuzzerPlay()` API

## 中斷與 ISR 注意事項
- UART1 IRQ：累積字元直到 newline，設定 `g_u8DataReady` 交由主迴圈解析
- GPB_IRQHandler：
  - PB7（HALL）僅做 edge 累積/flag 設定，不在 ISR 直接做 jump 數學運算
  - PB15（KeyA）設定 flag，主迴圈處理實際行為
- Timer0 ISR（`TMR0_IRQHandler`）以 1ms tick 觸發，更新系統 tick 與 LED callback

## 電源管理
- 支援 SPD（Shallow Power-Down）與 DPD（Deep Power-Down）
- `PowerMgmt_DetectUsbCharge()` 讀取 PA12 偵測 USB 充電；若為充電模式，系統進入 `PowerMgmt_RunChargeLoop()`（充電模式特殊行為）
- `PowerMgmt_DetectUsbCharge()` 讀取 PA12 偵測 USB 充電；充電模式建議使用 `PowerMgmt_ChargeModeInit()` / `PowerMgmt_ChargeModeProcess()`（非阻塞）
- 電源鎖（Power Lock）：PA11，高表示鎖定電源（保持開機），低表示允許關機
- SPD 進入前需將非必要 GPIO 設為輸入以降低漏電（`PowerMgmt_ConfigGpioForSPD`）
- Wake-up sources：PB15（按鍵, SPD）、PC0（DPD）等，Wake flag 由 `CLK->PMUSTS` 驗證

## 重要常數與可調參數（皆在 `project_config.h`）
- `PLL_CLOCK`：PLL 設定
- 移動/閒置判定參數：`MOVEMENT_SAMPLE_INTERVAL_MS`, `MOVEMENT_WINDOW_SAMPLES`, `MOVEMENT_STDDEV_THRESHOLD_G`, `NO_MOVEMENT_TIMEOUT_*`
- 電池量測閾值：`ADC_BATT_LOW_V`, `LOW_BATT_CHECK_INTERVAL_MS`
- G-sensor jump detect 參數（若啟用）

## 測試建議與檢查清單（開發/驗收時）
- 啟動流程：確認 BLE rename 流程、UART1 收發正常、UART0 debug 能顯示 log
- 電源模式測試：測試從 SPD / DPD 喚醒、測試 PB15 與 PC0 喚醒
- 充電模式：插拔 USB 檢查 `PowerMgmt_DetectUsbCharge()` 與 `PowerMgmt_ChargeModeProcess()` 的進出行為
- 跳繩計數：HALL 與 G-sensor 模式下各做 50 次跳繩比對
- 閒置關機：模擬停止移動並觀察 LED 與最終關機流程

## 單元測試（演算法層）
- 測試目標：
  - movement 判定：`GameAlgo_IsMovement`
  - jump 計數：`GameAlgo_CalcJumpsFromEdges`
- 位置：`SampleCode/RL_SPORT/tests/test_game_algorithms.c`
- 執行腳本：`SampleCode/RL_SPORT/tests/run_tests.ps1`

## 流程圖（Mermaid）

### 1) 啟動與主迴圈高階流程
```mermaid
flowchart TD
    A[Reset] --> B[RL_InitSystemCore]
    B --> C[RL_InitBoardInputs]
    C --> D[RL_InitDrivers]
    D --> E[RL_InitApplication]
    E --> F[BoardTest_RunAll]
    F --> G{while(1)}

    G --> H{USB charge mode?}
    H -- Yes --> H1[PowerMgmt_ChargeModeInit / Process]
    H1 --> G
    H -- No --> I[TestMode / USB HID test]
    I --> J[BLE msg + game state]
    J --> K[G-sensor / battery / key / hall event]
    K --> L[Idle power-off check]
    L --> M[LED update]
    M --> G
```

### 2) HALL edge -> jump 計數流程（ISR 減負）
```mermaid
flowchart LR
    IRQ[PB7 Falling Edge IRQ] --> ISR[GPB_IRQHandler]
    ISR --> ACC[Sys_AccumulateHallPb7Edge + set flag]
    ACC --> LOOP[Main Loop consume pending edges]
    LOOP --> CALC[GameAlgo_CalcJumpsFromEdges]
    CALC --> ADD[Sys_AddJumpTimes]
```

## 作為下一次專案的範例要點（最佳實務）
1. 模組化 API：保持 `xxx_Init()`, `xxx_Process()` 與 `xxx_ISR()` 介面清晰（見本專案模組化風格）。
2. 配置集中化：使用 `project_config.h` 管理所有可編譯旗標與參數，方便硬體差異化維護。
3. 最小 ISR 工作量：ISR 只設 flag 或做非常短的計數，實際邏輯在主迴圈或專門模組處理。
4. Timer 作為系統 tick：1ms tick + callback 方式管理 soft-timer、LED 更新、延遲等。避免在主迴圈使用 busy-wait。
5. 電源設計：在設計板級 MFP 與電源鎖時，確保 SPD/DPD 模式的腳位與 wake 設定在規格文件中描述清楚。
6. 測試驅動：加入自動板測函式 `BoardTest_RunAll()` 作為啟動健康檢查範例。

## 常見維護注意事項
- 若改變 G-sensor 型號或 I2C 位址，請同時更新 `gsensor.c` 與 `i2c` wrapper，並在 `project_config.h` 調整對應參數。
- BLE 模組 AT 命令回應字串若不同（不同廠牌），需更新 `ble.c` 中的 `BleCmdTable` 與解析邏輯。
- 若要在低功耗下保留 UART1 接收，需評估 BLE module 的低功耗協定（目前以 AT/暫停方式處理）。

---

如需我把此規格搬到專案根目錄或加上範例流程圖 (Mermaid)，我可以繼續補充。