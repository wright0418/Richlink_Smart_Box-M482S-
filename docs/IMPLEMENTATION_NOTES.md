Implementation notes & code review — BLE AT REPL (v2.1)

目的
------
修正與強化 BLE AT REPL 測試模式的穩定性與可解析性，並解決實機測試中觀察到的問題：
1. `AT+TEST,HELP` 回應被截斷且與下一個回應合併，造成 PC 端解析困難。
2. BLE 模組回傳可能混入標記 `!CCMD@`，污染 AT 輸入。
3. `LED_ON` / `LED_OFF` 指令曾被主程式的 blink engine 覆寫。
4. DFLASH 寫/擦操作需受限於 REPL 模式。

變更檔案與重點函式
--------------------
1) `SampleCode/RL_SPORT/ble_at_repl.c`
   - repl_send(fmt,...)
     - 原先 local buffer 為 192 bytes，長回應（如 HELP）會被截斷。
     - 改為 512 bytes 以容納當前需要的回應長度，避免截斷導致回應粘貼或合併。
   - handle_help()
     - 改用 `repl_send_ok("HELP", "...")` 輸出，保持回應格式一致且較易維護。
   - handle_dflash_*()
     - DFLASH 寫入/擦除操作會檢查 `Sys_GetReplMode()`；若未在 REPL，回 `+ERR,STATE,NOT_IN_REPL`。
   - handle_sensor_stream()
     - 若在非 REPL 收到 `SENSOR_STREAM,START`，會自動叫 `repl_activate()` 進入 REPL（設計上的特例）。

2) `SampleCode/RL_SPORT/ble.c`
   - CheckBleRecvMsg()
     - 新增迴圈移除接收行中出現的所有 `BLE_CMD_CCMD`（字串 `!CCMD@`），實作：
       while ((p = strstr(msg, BLE_CMD_CCMD)) != NULL) { memmove(p, p+strlen(BLE_CMD_CCMD), tail_len+1); }
     - 這樣可避免模組回傳的標記污染後續的 AT 字串解析（例如模組 echo、或多個 fragment 被合併時）。
   - 解析順序：先交給 `BleAtRepl_HandleMessage()` 處理 AT+TEST 前綴指令（回傳 1 表示已處理），否則繼續既有遊戲 BLE 解析流程。

3) `SampleCode/RL_SPORT/led.c`
   - 新增 `static volatile int8_t g_forceLedState = -1;`：當 >=0 時，代表 LED 被強制為該狀態（0 或 1），blink engine 不再覆寫。
   - SetGreenLed(uint8_t state)
     - 設定 `g_forceLedState` 並直接寫入實際引腳，供 REPL 的 `LED_ON` / `LED_OFF` 使用。
   - SetGreenLedMode(float freq, float duty)
     - 啟用 blinking 時會把 `g_forceLedState` 設回 -1（釋放強制控制），並以整數 ticks 計算 period/on-time（避免 ISR 使用浮點）。
   - Led_Update()
     - 如果 `g_forceLedState >= 0`，直接輸出該狀態並 return；否則繼續 blink 計數邏輯。

行為與設計要點
-----------------
- REPL 下執行 `LED_ON` / `LED_OFF`：流程會先 `SetGreenLedMode(0,0)` 停掉 blink engine，再呼 `SetGreenLed(0|1)` 並設定 `Sys_SetLedOverride(1)`，確保 LED 由 REPL 接管且不會被主迴圈或 ISR 覆寫。
- 呼叫 `SetGreenLedMode(freq>0, duty)`（例如 REPL `LED_BLINK`）時，會釋放 `g_forceLedState`（回到 blink engine 控制）並立即更新 LED 可見狀態。
- BLE 接收端會移除 `!CCMD@`（所有出現），因此模組若回傳該標記不會影響 AT 指令解析；若仍看到標記，通常是 UART chunking 或模組端在回傳中摻入非預期字串，需貼出原始 RX 行以便追查。

已解決的具體現象
------------------
1) HELP 被截斷：原因為 `repl_send` local buffer 太小（192 bytes）。已把 buffer 增為 512 bytes，並用 `repl_send_ok` 統一格式。
2) HELP 與 PING 合併顯示：由於回應被截斷且下一回應立即寫入，導致顯示粘連。擴大 buffer 與統一發送方式已修復該情形。
3) LED 指令被覆寫：透過 `g_forceLedState` 與 REPL 在進入時停用 blink engine，REPL 的 LED 指令現可穩定生效且持久。
4) 模組標記污染：在 `ble.c` 中移除所有 `!CCMD@`，避免 payload 污染。

測試與驗證建議
----------------
1. `AT+TEST,HELP` → 應收到完整 `+OK,HELP,<list>` 單行回應。
2. `AT+TEST,PING` → `+OK,PING,PONG`。
3. `AT+TEST,LED_ON` / `AT+TEST,LED_OFF` → LED 狀態持久，不被主閃爍覆蓋。
4. `AT+TEST,SENSOR_STREAM,START,GSENSOR,100` → 若在非 REPL，裝置會自動進入 REPL 並啟動串流。
5. `AT+TEST,DFLASH_WRITE,...` / `AT+TEST,DFLASH_ERASE` → 未在 REPL 時回 `+ERR,STATE,NOT_IN_REPL`，在 REPL 中可執行。

後續優化建議
---------------
- 日誌分級：將 `DBG_PRINT` 換成 leveled logging（INFO/WARN/ERROR/DEBUG），或在 `project_config.h` 新增 LOG_LEVEL 定義，以便長時間測試時降低噪音。
- 分段傳送大型回應：若未來回應變得更大，考慮把回應拆段發送或使用環形緩衝，避免 stack 上的大陣列或單一 vsnprintf 造成風險。
- 更健全的 UART 行處理：`CheckBleRecvMsg()` 目前以換行判定一行結束，對於 fragment 或多行回傳仍可能遇到邊界條件。可考慮 state-machine 或更嚴格的 framing（例如長度前綴或特定 start/end 標記）以提升穩定度。

變更紀錄
---------
- v2.1 (2026-03-07)
  - 修正 HELP 截斷（`repl_send` buffer -> 512 bytes）。
  - 在 `ble.c` 中移除所有 `!CCMD@` 標記。
  - LED 強制狀態支援（`g_forceLedState`），REPL 進入時停用主 blink engine。
  - DFLASH 寫/擦受限於 REPL。

附註
-----
若需，我可：
- 把上述內容同步回 `SampleCode/RL_SPORT/BLE_ATCmd_test_mode_design.md` 的 CHANGELOG（或改為簡短摘要並保留完整 notes 至此文件）。
- 開始將 `DBG_PRINT` 清理與分級（預估可減少 ~70% 的測試期輸出雜訊）。
- 協助將此檔加入 release note 或 PR 描述，並 push 到遠端分支。
