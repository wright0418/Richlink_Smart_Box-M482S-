# 目標
不改變原來遊戲設計，提供一個不破壞既有通訊流程的 BLE AT-style 測試模式（REPL），讓 PC 可透過 BLE 下指令做板上測試與除錯。

設計原則：
- 測試命令必須與遊戲通訊隔離（使用專屬前綴 `AT+TEST,`）。
- 非 `AT+TEST,` 的封包由原遊戲流程處理。
- 回應採 PC-friendly 格式（`+OK`, `+ERR`, `+DATA`），每行以 CRLF 結尾。

主要功能摘要：
- REPL 進入/退出：`AT+TEST,REPL_START` / `AT+TEST,REPL_STOP`；REPL 啟動後允許執行多項敏感操作。
- 查詢類：`AT+TEST,HELP`, `AT+TEST,PING`, `AT+TEST,VERSION`, `AT+TEST,STATUS`。
- 感測器串流：`AT+TEST,SENSOR_STREAM,START,<period_ms>` / `...,STOP`，啟動後會以 `+DATA,SENSOR,...` 週期推送。
- DFLASH 存取（受限）：讀取 `AT+TEST,DFLASH_READ,addr,len`、資訊 `AT+TEST,DFLASH_INFO`；寫入/擦除需在 REPL 模式且需顯式確認。
- 硬體控制：LED、Buzz、PWM、GPIO 等測試指令（例：`AT+TEST,LED,ON` / `LED,OFF`）。

回應格式（範例）：
- 成功：`+OK,<CMD>[,<PAYLOAD>]\r\n`
- 錯誤：`+ERR,<CODE>,<MSG>\r\n`
- 串流：`+DATA,<SRC>,...\r\n`

實作注意事項：
- RX 端會在解析前移除 BLE module 標記字串（例如 `!CCMD@`）以避免污染資料。
- HELP 的回覆較長，REPL 傳輸 buffer 已擴大以避免回覆被截斷或拼接。
- 對會影響運作的破壞性指令（DFLASH 寫/擦）只有在 REPL 模式才允許執行；否則返回 `+ERR,STATE,NOT_IN_REPL`。
- LED 命令會暫停 blink engine 的自動更新，並以 forced state 保持燈號直到 REPL 解除或顯式恢復。

更多實作細節與指令清單請參考 `docs/IMPLEMENTATION_NOTES.md`（包含回覆範例、錯誤碼表與行為說明）。
