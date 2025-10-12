# MDTS/MDTSG/MDTPG → PA.6 控制與 5 秒延長計時使用說明

本文件說明如何透過 Mesh 訊息 (MDTS/MDTSG/MDTPG) 的 payload 控制 `PA.6`，並使用「5 秒延長」的自動關閉機制。

## 功能規格

- 接收 MDTS/MDTSG/MDTPG 訊息後，解析 payload（十六進位字串）。
- 僅當 payload 轉為 bytes 的長度為 1 時才會執行控制：
  - 0x30 → OFF：`PA6 = 0`，同時清除計時。
  - 0x31 → ON：`PA6 = 1`，並將自動關閉截止時間設為「現在 + 5000ms」。
    - 若 5 秒內再次收到 0x31，會自動「延長 5 秒」。
- 逾時（超過截止時間）後，自動將 `PA6 = 0` 並清除計時。

## 互動與顯示

- 收到 MDTSG/MDTPG/MDTS：黃燈 (PB.2) 會分別快閃 1/2/3 次作為指示。
- 解析與控制邏輯位於 `SampleCode/Smart_Box/main.c` 的 `handle_mesh_line()` 與主迴圈逾時檢查。

## 與數位 I/O 測試的關係

- `digital_io_test()` 會根據 `PB.7` 輸入控制 `PA.6` 輸出。
- 為避免衝突，當 Mesh 的 `PA6` 延時計時啟動時（`g_pa6_auto_off_deadline_ms != 0`），主迴圈會暫停呼叫 `digital_io_test()`，避免覆蓋由 Mesh 指令設定的 `PA6`。
- 計時清除或逾時後，`digital_io_test()` 會恢復執行。

## 測試方法

1. 透過 BLE Mesh AT 對應通道送出下列訊息（舉例）：
   - `MDTSG-MSG <sender> <...> 31` → `PA6 = 1` 並啟動 5 秒計時。
   - `MDTSG-MSG <sender> <...> 30` → `PA6 = 0` 並清除計時。
   - `MDTPG-MSG <sender> 31` 或 `MDTS-MSG <sender> <...> 31` 亦可，依訊息型別擷取 payload。
2. 在 5 秒內重複發送 `31` 可延長開啟時間。
3. 不再發送 `31` 時，超過 5 秒會自動關閉 `PA6`。

注意：payload 必須是十六進位字串，且長度為 2（單一 byte），例如 `"30"` 或 `"31"`。傳送 ASCII 字元 '0' 或 '1' 並非有效的 0x30/0x31 表示。

## 相關常數與變數

- `PA6_ON_HOLD_MS`：延時毫秒數，預設 5000ms。
- `g_pa6_auto_off_deadline_ms`：自動關閉的截止時間（毫秒系統時基）。

## 例外與邊界情況

- 若 payload 長度不是 1（例如為空或多於 1 byte），不進行 `PA6` 控制。
- 當收到 `0x30` 時，無論目前是否在 ON/計時中，皆會立即關閉 `PA6` 並清除計時。
- 計時採用 `(int32_t)(now - deadline) >= 0` 比較方式，避免 32-bit 計數器溢位所造成的判斷錯誤。

## 建置與載入（VS Code 任務）

- CMSIS Load：建置並燒錄
- CMSIS Run：啟動 GDB 伺服器

這兩個任務已預先在工作區設定，可從 VS Code 的「執行任務」選單直接使用。
