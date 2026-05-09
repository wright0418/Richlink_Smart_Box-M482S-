# 專案配置與應用規格
使用 RL SPORT Board V3 進行開發，以下是專案配置和應用規格的詳細說明：

## 分支聲明（Mole-only）
- 本分支為 Mole（認知訓練/打地鼠）專用，不會回合 `main`。
- BLE 裝置命名前綴固定為 `MOLE_XXXX`。
- 分支政策請見：`../../docs/SampleCode/RL_SPORT/MOLE_BRANCH_POLICY.md`

## 維護與驗證計畫
後續系統整理、重構順序與每一步驗證門檻，請參考：`../../docs/SampleCode/RL_SPORT/REFACTORING_VALIDATION_PLAN.md`

## 快速驗證入口

### 1. 韌體建置
- CMSIS solution：`VSCode/RL_SPORT.csolution.yml`
- CMSIS project：`VSCode/RL_SPORT.cproject.yml`
- 目前專案 target 為 AC6 / ARMCLANG。

### 2. Host-side 演算法測試
- 測試檔：
  - `tests/test_game_algorithms.c`
  - `tests/test_ble_parser.c`
- parser module：`protocol/ble_parser.c`, `protocol/ble_parser.h`
- 腳本：`tests/run_tests.ps1`
- 說明：
  - 若電腦有 host `gcc` 或 `clang`，腳本會編譯並執行測試。
  - 若只有 `arm-none-eabi-gcc` 或 `armclang`，則只做 compile-only check。
  - 目前腳本會驗證 2 個 target：演算法測試與 BLE parser 測試。

### 3. 板上測試入口

本專案有兩條測試通道，請不要混用：

| 通道 | 傳輸介面 | 命令前綴 | 用途 |
| --- | --- | --- | --- |
| UART0 板測 / 產線測試 | `UART0` | `AT+TEST=` | 板級自動測試、互動式 test menu |
| BLE REPL | `UART1` 經 BLE module | `AT+TEST,` | 遠端讀狀態、感測器/LED/Buzzer/DFLASH 測試 |

#### UART0 測試模式
- 在 UART0 輸入 `test`：進入數字選單互動測試。
- 在 UART0 傳送 `AT+TEST=<CMD>\r\n`：執行結構化自動測試。
- 入口實作：`test_mode.h`, `test_mode.c`
- 協定文件：`../../docs/UART_AUTO_TEST_PROTOCOL.md`

常用範例：
- `AT+TEST=INFO`
- `AT+TEST=ALL`
- `AT+TEST=BLE,NAME`
- `AT+TEST=BLE,MAC`

#### BLE REPL 測試模式
- 傳送 `AT+TEST,REPL_START` 啟用 REPL。
- 傳送 `AT+TEST,REPL_STOP` 離開 REPL。
- 入口實作：`ble_at_repl.h`, `ble_at_repl.c`

常用範例：
- `AT+TEST,PING`
- `AT+TEST,VERSION`
- `AT+TEST,STATUS`
- `AT+TEST,SENSOR_READ`
- `AT+TEST,DFLASH_INFO`

## 建議的最小回歸流程
1. 先確認韌體可成功建置。
2. 執行 `tests/run_tests.ps1`，至少確認 compile-only check 成功。
3. 燒錄後先跑 UART0：`AT+TEST=INFO`。
4. 再跑 UART0：`AT+TEST=ALL`。
5. 檢查 BLE rename / connect / disconnect / `get cycle` / `set end` 流程。
6. 若需遠端診斷，再用 BLE REPL 進行狀態與感測器讀取。


## 跳繩第三版
  - 跳繩次數只透過 Hall Sensor GPIO 中斷計數 (一圈會觸發兩次)，G-sensor 不參與跳繩偵測
  - 判斷 gsensor 是否靜止不動 60 秒，確認後 關機
  - 一啟動系統 先使用 Power Lock GPIO 鎖住電源開關
  - 使用 LED 表示系統狀態 
  - 使用 Buzzer 通知遊戲狀態
  - Buzzer 使用 Timer 中斷製作方波頻率 發出聲音，關閉時GPIO需要為 Low ,
  - ADC 量測 低電壓 並且 使用快閃Led 通知 低電壓需充電
  - 開機後檢查 BLE NAME 是否已經被改成 `MOLE_` 開頭，若否則進行改名
  - BLE game 開始進入 DATA MODE ，方便傳輸
  - BLE UART 收到 "get cycle\r\n" 就開始遊戲 ，需要發出 Buzzer 告知使用者開始遊戲
  - BLE UART 收到 "set end\r\n" 遊戲結束，發出 Buzzer 提示
  - BLE 送出 "send 次數\r\n" 給 APP
  - USB 插入充電自動開機時，PA12 會為 High，進入充電模式：不做 Power Lock、不進入遊戲，停在獨立 while(1)
