# RL_SPORT V3 板測指南

此文件描述目前 `RL_SPORT` 韌體中的完整板級測試流程，內容已對齊現行 I/O 規劃與功能配置。

## 目的

`BoardTest_RunAll()` 是一套 **full board test**：

- 對齊目前實際使用的 I/O 腳位
- 同時涵蓋
  - **人工確認項目**：LED、Buzzer、WS2812 顯示
  - **互動項目**：KEYA、KEYB、HALL
  - **自動判斷項目**：Power Lock / VBUS、VDDA、IMU 6-axis
- 可在開機自動執行，也可從 UART0 測試選單進入

## 目前板測對齊的 I/O / 功能

- `PB2`：Yellow LED
- `PB3`：Green LED
- `PB15`：KEYA
- `PC0`：KEYB
- `PB7` / `PB8`：HALL inputs
- `PC7`：Buzzer
- `PC5`：IMU interrupt input
- `PF6`：WS2812 data (`SPI0_MOSI`)
- `PA11`：Power Lock
- `PA12`：USB VBUS detect
- `PB4` / `PB5`：I2C0 (IMU)
- `PB12` / `PB13`：UART0 測試埠
- `PA8` / `PA9`：UART1 / BLE module

> 注意：目前電壓檢查不是量 `PB1` 外部 ADC，而是使用 **EADC band-gap** 反推 `VDDA`。

## Full Board Test 流程

執行 `BoardTest_RunAll()` 時，UART0 會印出：

- `=== RL_SPORT V3 Board Test ===`
- `[BT] FLOW=FULL ...`
- 各測項結果
- 最後摘要：`[BT] SUMMARY: PASS=x FAIL=y MANUAL=z SKIP=w`

### 測項順序

1. `LED_GY`
   - 依序顯示：Green → Yellow → Both
   - 腳位：`PB3`, `PB2`
   - 類型：**MANUAL**

2. `BUZZER_PC7`
   - 連續播放兩個不同音高的 beep
   - 腳位：`PC7`
   - 類型：**MANUAL**

3. `WS2812_PF6`
   - 顯示：Red → Green → Blue → Rainbow
   - 腳位：`PF6`
   - 類型：**MANUAL**（但韌體也會檢查 refresh 是否成功）

4. `KEYA_PB15`
   - 等待 `KEYA` 按下
   - 腳位：`PB15`
   - 類型：**PASS/FAIL**

5. `KEYB_PC0`
   - 等待 `KEYB` 按下
   - 腳位：`PC0`
   - 類型：**PASS/FAIL**

6. `HALL_PB7_PB8`
   - 等待磁鐵或治具造成 HALL 狀態變化
   - 腳位：`PB7`, `PB8`
   - 類型：**PASS/FAIL**

7. `POWER_PA11_PA12`
   - 讀取 `PA11`（Power Lock）與 `PA12`（VBUS）
   - `PA11` 應維持 High
   - 類型：**PASS/FAIL**

8. `VDDA_BG`
   - 以 internal band-gap 量測 `VDDA`
   - 目前判定範圍：`2.0V ~ 4.0V`
   - 類型：**PASS/FAIL**

9. `IMU_I2C_PC5`
   - 執行 6-axis static test
   - 使用 I2C 與 IMU 裝置通訊，並列印 `PC5` 狀態
   - 類型：**PASS/FAIL**

## 判定方式

### MANUAL 項目

以下測項會列為 `MANUAL`，需人眼 / 人耳確認：

- `LED_GY`
- `BUZZER_PC7`
- `WS2812_PF6`

### PASS / FAIL 項目

- `KEYA_PB15`：timeout 內必須偵測到 active-low press
- `KEYB_PC0`：timeout 內必須偵測到 active-low press
- `HALL_PB7_PB8`：timeout 內至少要有 1 次狀態變化
- `POWER_PA11_PA12`：`PA11` 必須為 high；`PA12` 只列印當下狀態
- `VDDA_BG`：`VDDA` 必須落在 `2.0V ~ 4.0V`
- `IMU_I2C_PC5`：6-axis static 指標需通過（含 comm fail / magnitude / stddev / gyro mean）

## 如何觸發板測

### 方式 1：開機自動執行

在 `SampleCode/RL_SPORT/project_config.h`：

- `#define BOARD_TEST_AUTORUN 1`

系統開機後會自動執行 `BoardTest_RunAll()`。

### 方式 2：UART0 測試選單

1. 連接 `UART0`（`PB12/PB13`），115200 8N1
2. 傳送：`test\r`
3. 選擇：`7`

`7) Run full board test`

此選項現在會直接呼叫與開機相同的 `BoardTest_RunAll()`。

## UART0 測試選單（目前對齊版）

- `1) LED PB2/PB3`
- `2) Buzzer PC7`
- `3) Key KEYA(PB15) / KEYB(PC0)`
- `4) HALL PB7/PB8`
- `5) G-sensor I2C`
- `6) VDDA (band-gap)`
- `7) Run full board test`
- `8) USB FS HID Mouse (auto 5s)`
- `9) BLE AT CMD name check`
- `a) WS2812 16x16 rainbow test`
- `b) IMU 6-axis static test`
- `0) Exit`

## 延伸測項（不在 boot full board test 中）

以下測項仍建議從 UART test mode / `AT+TEST=` 執行：

- BLE name / MAC 檢查（UART1 / BLE 模組）
- USB HID Mouse 測試
- 單項 AT 自動化測試（`AT+TEST=...`）

理由：

- 這些項目通常需要外部主機或 BLE 模組狀態配合
- 不適合塞進每次上電都要跑的 boot board test 主流程

## 建議治具流程

若是產線或板端 bring-up，建議順序如下：

1. 上電開機
   - 可選：按住 `KEYB(PC0)` 約 0.8 秒，直接進入 UART test mode（不用先輸入 `test`）
2. 執行 `BoardTest_RunAll()`
3. 人工確認：
   - LED 顯示
   - Buzzer 聲音
   - WS2812 畫面
4. 依序觸發：
   - KEYA
   - KEYB
   - HALL
5. 觀察 UART0 摘要：`FAIL=0`
6. 需要時再進 UART test mode 執行：
   - BLE name check
   - USB test
   - 單項 AT 測試

## 相關檔案

- 板測主流程：`SampleCode/RL_SPORT/board/board_test_gpio.c`
- 板測 API：`SampleCode/RL_SPORT/board/board_test_gpio.h`
- UART 測試選單：`SampleCode/RL_SPORT/test_mode.c`
- GPIO / Power：`SampleCode/RL_SPORT/board/gpio.c`, `board/power_mgmt.c`
- 顯示 / 蜂鳴器 / 感測器驅動：
  - `SampleCode/RL_SPORT/drivers/led.c`
  - `SampleCode/RL_SPORT/drivers/buzzer.c`
  - `SampleCode/RL_SPORT/drivers/ws2812b.c`
  - `SampleCode/RL_SPORT/drivers/gsensor.c`
  - `SampleCode/RL_SPORT/drivers/adc.c`

## 補充

如果未來 I/O 再變更，請至少同步更新這 3 個地方：

1. `board_test_gpio.c`
2. `test_mode.c`
3. 本文件 `BOARD_TEST_README.md`
