BLE AT 指令使用手冊 (RL_SPORT)

概覽
- 指令前綴：`AT+TEST,`（所有測試指令必須以此開頭）
- 每行以 CRLF 結尾（"\r\n"）
- 回應類型：
  - 成功：`+OK,<CMD>[,<PAYLOAD>]\r\n`
  - 錯誤：`+ERR,<CODE>,<MSG>\r\n`
  - 串流資料：`+DATA,<SRC>,...\r\n`

通用錯誤碼說明
- `STATE`：目前狀態不允許（例如未進入 REPL）
- `PARAM`：參數錯誤或缺失
- `CMD`：未知指令
- `BUSY`：資源忙碌（例如串流已在執行）
- `FLASH`：Data-Flash 操作失敗
- `RANGE`：位址或長度超出範圍
- `ALIGN`：位址未 4-byte 對齊

資料快照與限制
- Data Flash 基址/大小（實作）：`DFLASH_BASE=0x0007F000`, `DFLASH_SIZE=0x1000`（4 KB）
- DFLASH 單次讀寫最大長度：`DFLASH_MAX_RW_LEN=64` (word count)
- SENSOR 串流間隔限制：50 ms <= interval <= 2000 ms

命令清單（語法、用途、範例請求 & 範例回應）

1) REPL 模式控制
- 語法：`AT+TEST,REPL_START`
  - 目的：進入 REPL 測試模式（會停止主流程並暫停 LED blink engine）
  - 範例回應：`+OK,REPL_START,READY\r\n`

- 語法：`AT+TEST,REPL_STOP`
  - 目的：離開 REPL，清除 REPL 期間的 LED override
  - 範例回應：`+OK,REPL_STOP,BYE\r\n`

2) 幫助與資訊
- 語法：`AT+TEST,HELP`
  - 目的：列出支援指令清單
  - 範例回應：`+OK,HELP,<CMDLIST>\r\n` (CMDLIST 為以 "|" 分隔的指令)

- 語法：`AT+TEST,PING`
  - 目的：連通性檢查
  - 範例回應：`+OK,PING,PONG\r\n`

- 語法：`AT+TEST,VERSION`
  - 目的：韌體版本與編譯時間
  - 範例回應：`+OK,VERSION,1.0.0,MMM DD YYYY,HH:MM:SS\r\n`

3) 狀態查詢
- 語法：`AT+TEST,STATUS`
  - 目的：回傳簡短系統狀態 (BLE、Game、REPL、Idle、JumpTimes)
  - 範例回應：`+OK,STATUS,BLE=1,GAME=0,REPL=1,IDLE=0,JUMP=3\r\n`

- 語法：`AT+TEST,STATUS_VERBOSE`
  - 目的：回傳詳細狀態 (含 ADC、VBAT、按鍵、G sensor 等)
  - 範例回應（範例分兩行）：
    - `+OK,STATUS_VERBOSE,BLE=1,GAME=0,REPL=1,IDLE=0,JUMP=3,KEY=0,PB7=1,PB8=1,ADC=1023,VBAT=3.700\r\n`
    - `+OK,STATUS_VERBOSE_G,AX=12,AY=34,AZ=-56\r\n`

- 語法：`AT+TEST,REPL_STATE`
  - 目的：回傳 REPL 相關狀態（是否 active、串流是否啟用、來源、間隔）
  - 範例回應：`+OK,REPL_STATE,ACTIVE=1,STREAM_EN=1,SRC=GSENSOR,INT=200\r\n`

4) LED 控制
- 語法：`AT+TEST,LED_ON`
  - 目的：立即開啟綠燈並設定為 override 模式（暫停主 blink engine）
  - 範例回應：`+OK,LED_ON,1\r\n`

- 語法：`AT+TEST,LED_OFF`
  - 目的：立即關閉綠燈並設定為 override 模式
  - 範例回應：`+OK,LED_OFF,0\r\n`

- 語法：`AT+TEST,LED_BLINK[,<freq>[,<duty>]]`
  - 目的：以指定頻率 (Hz) 與佔空比控制 LED（若未提供，預設 freq=2.0, duty=0.5）
  - 範例：`AT+TEST,LED_BLINK,1.5,0.25`
  - 成功回應：`+OK,LED_BLINK,APPLIED\r\n`
  - 參數錯誤回應：`+ERR,PARAM,LED_BLINK_FREQ\r\n`

5) Buzzer 控制
- 語法：`AT+TEST,BUZZER_ON`
  - 目的：啟動 buzzer（預設頻率 1000 Hz）
  - 回應：`+OK,BUZZER_ON,1000\r\n`

- 語法：`AT+TEST,BUZZER_OFF`
  - 目的：停止 buzzer
  - 回應：`+OK,BUZZER_OFF,0\r\n`

- 語法：`AT+TEST,BUZZER_BEEP[,<freq>,<dur_ms>]`
  - 目的：以指定頻率與時間播放單次 beep（預設 1500Hz, 150ms）
  - 範例：`AT+TEST,BUZZER_BEEP,2000,300`
  - 成功回應：`+OK,BUZZER_BEEP,DONE\r\n`
  - 參數錯誤回應：`+ERR,PARAM,BUZZER_BEEP\r\n`

6) 感測器讀取與串流
- 語法：`AT+TEST,SENSOR_READ`
  - 目的：讀取三軸加速度（GSENSOR）
  - 範例回應：`+OK,SENSOR_READ,AX,AY,AZ\r\n`（整數值）

- 語法：`AT+TEST,SENSOR_STREAM,QUERY`
  - 目的：查詢現在串流狀態
  - 範例回應：`+OK,SENSOR_STREAM,QUERY,EN=1,SRC=GSENSOR,INT=200\r\n`

- 語法：`AT+TEST,SENSOR_STREAM,STOP`
  - 目的：停止目前的 sensor 串流
  - 範例回應：`+OK,SENSOR_STREAM,STOP\r\n`

- 語法：`AT+TEST,SENSOR_STREAM,START,<SRC>[,<interval_ms>]`
  - 目的：啟動感測器週期性推送，來源 `<SRC>` 可為 `GSENSOR`/`SENSOR`、`ADC`、`HALL`、`KEY`
  - `interval_ms` 為毫秒週期（若省略使用預設，並會被 clamp 到 50–2000 ms）
  - 範例：`AT+TEST,SENSOR_STREAM,START,GSENSOR,100`
  - 成功回應：`+OK,SENSOR_STREAM,START,GSENSOR,100\r\n`
  - 串流推送範例（主動送出）：
    - `+DATA,GSENSOR,AX,AY,AZ\r\n`
    - `+DATA,ADC,RAW,VBAT\r\n`
    - `+DATA,HALL,PB7=0,PB8=1\r\n`
    - `+DATA,KEY,PB15=0\r\n`

7) ADC / HALL / KEY 讀取
- `AT+TEST,ADC_READ` -> `+OK,ADC_READ,<raw>,<vbat>\r\n` (vbat 有小數)
- `AT+TEST,HALL_READ` -> `+OK,HALL_READ,PB7=<0|1>,PB8=<0|1>\r\n`
- `AT+TEST,KEY_READ` -> `+OK,KEY_READ,PB15=<0|1>\r\n`

8) Data-Flash (DFLASH) 相關（部分操作需在 REPL 模式才能執行）
- 語法：`AT+TEST,DFLASH_INFO`
  - 目的：列出 Data-Flash 基本資訊
  - 回應：`+OK,DFLASH_INFO,BASE=0xXXXXXXXX,SIZE=0xXXXX,PAGE=0xXXXX\r\n`

- 語法：`AT+TEST,DFLASH_READ,<hex_offset>[,<word_count>]`
  - 目的：讀取 Data-Flash（offset 以 hex，為相對 DFLASH_BASE 的位移；count 為 word 數，預設 1）
  - 範例請求：`AT+TEST,DFLASH_READ,0x0,4`
  - 成功回應（每個 word 一行）：`+OK,DFLASH_READ,0x0000,0x12345678\r\n`
  - 可能錯誤：`+ERR,ALIGN,DFLASH_READ_OFFSET`、`+ERR,RANGE,DFLASH_READ_COUNT` 等

- 語法：`AT+TEST,DFLASH_WRITE,<hex_offset>,<hex_value>` (僅在 REPL 可執行)
  - 目的：寫入單一 word（4 byte）到 Data-Flash
  - 範例：`AT+TEST,DFLASH_WRITE,0x00,0xDEADBEEF`
  - 成功回應：`+OK,DFLASH_WRITE,0x0000,0xDEADBEEF\r\n`
  - 錯誤（非 REPL）：`+ERR,STATE,NOT_IN_REPL\r\n`
  - 參數錯誤範例：`+ERR,PARAM,DFLASH_WRITE_PARAMS\r\n`

- 語法：`AT+TEST,DFLASH_ERASE` (僅在 REPL 可執行)
  - 目的：擦除 Data-Flash 頁面
  - 成功回應：`+OK,DFLASH_ERASE,DONE\r\n`
  - 錯誤（非 REPL）：`+ERR,STATE,NOT_IN_REPL\r\n`

9) 未知或不允許使用時的行為
- 當接收到非 `AT+TEST,` 前綴封包時，REPL 會忽略該封包（交由原遊戲流程處理）。
- 對於需要 REPL 的破壞性操作（DFLASH 寫/擦），若未進入 REPL，會回傳：`+ERR,STATE,NOT_IN_REPL\r\n`。
- 當指令不支援或參數錯誤時，會以 `+ERR,<CODE>,<MSG>\r\n` 回應，`<MSG>` 字串為簡短的錯誤目標標籤（例如 `DFLASH_WRITE_PARAMS`）。

實務與測試建議
- 在執行 `DFLASH_WRITE` / `DFLASH_ERASE` 前，務必 `AT+TEST,REPL_START` 並確認回應為 `+OK,REPL_START,READY`。
- 使用 `AT+TEST,HELP` 取得最新可用指令清單。
- SENSOR 串流測試：先以 `AT+TEST,SENSOR_STREAM,START,GSENSOR,100` 啟動，再觀察 `+DATA` 行為。要停止請使用 `AT+TEST,SENSOR_STREAM,STOP`。

檔案參考
- 實作來源：`SampleCode/RL_SPORT/ble_at_repl.c`


