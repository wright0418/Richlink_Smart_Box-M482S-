# RL_SPORT 板上測試指南 (UART0 測試模式)

目的
- 提供一個透過序列埠 (UART0) 進入的互動式板上測試選單，讓 PC 測試工具可以自動或手動逐項檢查硬體周邊（LED、蜂鳴器、按鍵、HALL、G-sensor、ADC、USB、BLE）。

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
- 選單會顯示數字 (0-9) 選項，使用者在提示 `Select>` 後輸入數字並按 Enter。
- 常見選項（以韌體實作為準）:
   1) LED PB3
     - 行為：閃爍 PB3 三次。
     - 輸出範例：`[Test] LED PB3 blink x3`，不會有機器可解析的結果，屬人工檢查項目。

  2) Buzzer PC7
     - 行為：播放一次 beep。
     - 輸出範例：`[Test] Buzzer PC7 beep`。

  3) Key PB15
     - 行為：等待 5 秒內按下 PB15，若偵測到輸出 `Key pressed`，否則回傳 `Key TIMEOUT`。
     - 可由 PC 驅動按鍵控制 (若具備外部按鍵控制治具)。

  4) HALL PB7/PB8
     - 行為：連續讀取 3 秒，偵測 PB7/PB8 變化並輸出變動值。
     - 輸出範例：`[Test] PB7=0 PB8=1`。

  5) G-sensor I2C
     - 行為：讀取 3 組三軸資料並顯示。
     - 輸出範例：`[Test] XYZ = 12, 34, -56`。

  6) ADC PB1 (battery)
     - 行為：讀取電池 ADC 平均值並回傳原始值與電壓。
     - 輸出範例：`[Test] raw=1023 V=3.70V`。

  7) Run all tests
     - 會依序執行 LED / Buzzer / Key / HALL / G-sensor / ADC 等（部分為人工檢查）。

  8) USB FS HID Mouse (auto 5s)
     - 行為：啟動板上 USB HID Mouse 測試流程自動執行 5 秒。
     - 輸出範例：`[Test] USB auto test done`。

  9) BLE AT CMD name check
     - 行為：切換 BLE 模組到 CMD 模式，查詢名稱並判斷回傳是否包含預期字首 (如 `ROPR_` 或 `ROPE_`)。
     - 成功回傳範例：`[Test] BLE_AT_CMD PASS`。失敗會列出失敗原因。

  0) Exit
   - 行為：離開測試模式並回到主程式循環。

自動化測試工具要點（PC 端）
- 先開啟序列埠 (115200 8N1)，確保能讀寫 CR 或 CRLF。
- 步驟範例：
   1) 傳送 `test\r`。
   2) 等待並解析選單輸出（可使用 timeout ，例如 1s）。
   3) 傳送選項號（例如 `9`）再加上 `\r`。
   4) 監聽並解析測試回應；視項目不同回應可能是可解析的資料（ADC、G-sensor、HALL）或狀態字串（PASS/FAIL/TIMEOUT/MANUAL CHECK）。

回應字串與格式約定
- 所有板上測試輸出皆使用 `printf()`，會回傳可讀的 ASCII 行（以 `\n` 或 `\r\n` 結尾），常見前綴：`[Test]`、`[BT]`。
- 自動化解析應採行為驅動（關鍵字 match）：
   - 成功或主要資訊通常以 `PASS`、`raw=`、`V=`、`XYZ =`、`PB7=` 等關鍵字出現。
   - 人工檢查項目會標示 `MANUAL CHECK`，需人工確認或配合外部儀器。

實務注意事項與硬體設定
- 進入測試模式需要在 UART0 接收到 `test` 並送出換行；如果主程式已經大量使用 UART0 做其他工作，請在進入測試前暫停或確保序列埠交互不會被搶佔。
- Key (PB15) 必須設為 Quasi mode（韌體預設於 `Gpio_Init()` 與 `BoardTest_GPIO_Init()` 已設定）。
- Power Lock (PA11) 與充電行為：韌體在啟動時會偵測 USB 充電，若偵測到則會進入充電模式（不啟動遊戲），且 `PowerMgmt_ChargeModeInit()` 會在必要時將 PA11 設為可驅動狀態以釋放/控制電源鎖（實作請參考 `power_mgmt.c`）。若測試需要控制/驗證 power lock，請注意此腳位為輸出模式。

範例 PC 測試腳本摘錄（流程摘要）
1) 開啟 COM 與 115200 8N1
2) 傳送 `test\r`，等待選單
3) 傳送 `9\r` 執行 BLE 名稱檢查
4) 監聽回應 1s：若含 `BLE_AT_CMD PASS` 表示通過，否則記錄錯誤訊息
5) 傳送 `0\r` 離開

檔案與實作參考
- 測試模式實作：`SampleCode/RL_SPORT/test_mode.c`（包含選單、每項測試函式與 BLE 測試流程）
- GPIO 板級測試：`SampleCode/RL_SPORT/board_test_gpio.c`
- 電源 / 充電檢測：`SampleCode/RL_SPORT/power_mgmt.c`

建議
- 若要自動化全部測項，建議在硬體測試夾具上加上可控按鍵與檢測回授點（例如 LED 回讀或 ADC 閾值），以把人工檢查項目改為可自動判斷。

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
Board test prints simple English logs:

- `[BT] <ITEM>: PASS`
- `[BT] <ITEM>: FAIL - <hint>`
- `[BT] <ITEM>: SKIP`
- Final summary: `[BT] SUMMARY: PASS=x FAIL=y SKIP=z`

Current PASS/FAIL criteria
--------------------------
- `LED`: manual check (must blink 3 times)
- `BUZZER`: manual check (must beep 2 times)
- `POWER_LOCK`: removed from board test (not tested here)
- `BATTERY_ADC`: battery voltage must be in 2.0V ~ 5.5V
- `GSENSOR_I2C`: read and print XYZ values for 3 samples
- `BLE_AT_NAME`: removed from board test (use Test Mode item `9`)
- `KEY`: optional test item (default SKIP in quick run)

Operator test steps (recommended)
---------------------------------
1. Run board test and watch UART logs.
2. Check LED and buzzer physically.
3. If fail appears, follow hint text directly:
   - `BATTERY_ADC` fail: check PB1 divider and ADC path
4. Confirm final summary has `FAIL=0`.

Test Mode BLE AT CMD
--------------------
Use UART test mode item `9) BLE AT CMD name check` for BLE name query/check.

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
