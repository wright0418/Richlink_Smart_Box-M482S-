# RL_SPORT 板上測試指南 (UART0 測試模式)

目的
- 提供一個透過序列埠 (UART0) 進入的互動式板上測試選單。
- 需先「進入 test mode」後，才可點選測試項目。
- 目前僅開放：Buzzer、Key、HALL、G-sensor、ADC、BLE。

通訊設定（PC 端）
- Baud: 115200
- Data: 8 bits
- Parity: None
- Stop: 1
- 換行: CR 或 CRLF（按下 Enter 即可送出）

如何進入 UART 測試模式
- 接上電源後於 UART0（通常對應開發板的 USB-UART）送出純文字 `test` 並以 Enter 結尾（`\r` 或 `\r\n`）。
- 韌體會在背景輪詢 UART0，如果接收到一行等於 `test`，會切換到測試選單模式並回傳選單文字。
- 範例：PC 傳送 `test\r` -> 板子回傳 `=== UART0 Test Mode ===` 與選單清單。

測試選單與對應行為
- 選單會顯示數字 (0-6) 選項，使用者在提示 `Select>` 後輸入數字並按 Enter。
- 常見選項（以韌體實作為準）:
   1) Buzzer PC7
     - 行為：播放一次 beep。
     - 輸出範例：`[Test] Buzzer PC7 beep`。

  2) Key PB15
     - 行為：等待 5 秒內按下 PB15，若偵測到輸出 `Key pressed`，否則回傳 `Key TIMEOUT`。
     - 可由 PC 驅動按鍵控制 (若具備外部按鍵控制治具)。

  3) HALL PB7/PB8
     - 行為：連續讀取 3 秒，偵測 PB7/PB8 變化並輸出變動值。
     - 輸出範例：`[Test] PB7=0 PB8=1`。

  4) G-sensor I2C
     - 行為：讀取 3 組三軸資料並顯示。
     - 輸出範例：`[Test] XYZ = 12, 34, -56`。

  5) ADC PB1 (battery)
     - 行為：讀取電池 ADC 平均值並回傳原始值與電壓。
     - 輸出範例：`[Test] raw=1023 V=3.70V`。

  6) BLE AT CMD name query (raw)
     - 行為：切換 BLE 模組到 CMD 模式並查詢名稱。
     - 不做 PASS/FAIL 判斷，直接顯示原始名稱回應。

  0) Exit
   - 行為：離開測試模式並回到主程式循環。

自動化測試工具要點（PC 端）
- 先開啟序列埠 (115200 8N1)，確保能讀寫 CR 或 CRLF。
- 步驟範例：
   1) 傳送 `test\r`。
   2) 等待並解析選單輸出（可使用 timeout ，例如 1s）。
   3) 傳送選項號（例如 `6`）再加上 `\r`。
   4) 監聽並解析測試回應；以原始回應文字為準。

回應字串與格式約定
- 所有板上測試輸出皆使用 `printf()`，會回傳可讀的 ASCII 行（以 `\n` 或 `\r\n` 結尾），常見前綴：`[Test]`、`[BT]`。
- 自動化解析應採行為驅動（關鍵字 match）：
   - 目前不做 PASS/FAIL 判斷，請直接使用原始回應內容（例如 `raw=`、`V=`、`XYZ =`、`PB7=`、`BLE RAW NAME =`）。

實務注意事項與硬體設定
- 進入測試模式需要在 UART0 接收到 `test` 並送出換行；如果主程式已經大量使用 UART0 做其他工作，請在進入測試前暫停或確保序列埠交互不會被搶佔。
- Key (PB15) 必須設為 Quasi mode（韌體預設於 `Gpio_Init()` 與 `BoardTest_GPIO_Init()` 已設定）。
- Power Lock (PA11) 與充電行為：韌體在啟動時會偵測 USB 充電，若偵測到則會進入充電模式（不啟動遊戲），且 `PowerMgmt_ChargeModeInit()` 會在必要時將 PA11 設為可驅動狀態以釋放/控制電源鎖（實作請參考 `power_mgmt.c`）。若測試需要控制/驗證 power lock，請注意此腳位為輸出模式。

範例 PC 測試腳本摘錄（流程摘要）
1) 開啟 COM 與 115200 8N1
2) 傳送 `test\r`，等待選單
3) 傳送 `6\r` 執行 BLE 名稱查詢
4) 監聽回應 1s：擷取並記錄原始 BLE 回應
5) 傳送 `0\r` 離開

檔案與實作參考
- 測試模式實作：`SampleCode/RL_SPORT/test_mode.c`（包含選單、每項測試函式與 BLE 測試流程）
- GPIO 板級測試：`SampleCode/RL_SPORT/board_test_gpio.c`
- 電源 / 充電檢測：`SampleCode/RL_SPORT/power_mgmt.c`

建議
- 若要自動化測項，建議以關鍵字擷取原始回應並儲存 log，方便後續追蹤。

若需我幫忙：
- 我可以補一個範例 Python/serial 腳本 (pyserial) 做完整自動測試流程範本，或將 `test_mode` 的輸出標準化為機器易解析的 JSON-like 行，您要哪一種？

RL_SPORT V3 - Board Test Guide
==============================

Overview
--------
This board test is a quick hardware check for RL_SPORT V3.

Current test items:

- LED (PB3)
- Buzzer (PC7)
- Battery ADC (PB1 / EADC0_CH1)
- G-sensor I2C XYZ read (3 samples)
- Key (PB15) is optional interactive test (not in default auto sequence)

Boot behavior
-------------
- Firmware always gives one short boot beep.
- `BoardTest_RunAll()` is optional and controlled by `BOARD_TEST_AUTORUN` in `project_config.h`:
  - `0`: do not auto-run board test at boot (default)
  - `1`: run board test once at boot

Pin mode rule (important)
-------------------------
- PB15 uses **Quasi mode** in app flow.
- PA11 (Power Lock) uses **Output mode** only.

How to run
----------
1. Build `SampleCode/RL_SPORT`.
2. Connect UART0 (PB12/PB13), 115200-8-N-1.
3. Choose one method:
   - Set `BOARD_TEST_AUTORUN = 1` and reboot, or
   - Call `BoardTest_RunAll()` from a debug path/test entry.

UART log format
---------------
Board test prints raw UART logs (no PASS/FAIL judgment):

- `[Test] ...`
- `[BT] ...`
- `BLE RAW NAME = ...`

Operator test steps (recommended)
---------------------------------
1. Enter test mode with `test` + Enter.
2. Select test items from menu (Buzzer/Key/HALL/G-sensor/ADC/BLE).
3. Record raw UART response directly for analysis.

Test Mode BLE AT CMD
--------------------
Use UART test mode item `6) BLE AT CMD name query (raw)` for BLE name query.

Key test notes (PB15)
---------------------
If key test reports pressed without pressing:

1. Run test with no touch for full timeout.
2. Measure PB15 level:
   - idle should stay HIGH
   - press should go LOW
3. If unstable, check key pull-up path and board noise coupling.

Integration example
-------------------
```c
int main(void)
{
    SYS_Init();
    UART_Open(UART0, 115200);
    BoardTest_RunAll();
    while (1) { }
}
```
