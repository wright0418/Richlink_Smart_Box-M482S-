# 目標
 不改變 原來遊戲設計，可以透過 BLE 接收 PC  BLE 送下 command 進入測試模式可以對 電路板做各種互動(測試或者功能切換)

 # 功能
 1. 設計相關的通訊規則 , 不要干擾到現在遊戲的通訊規則
 2. 設計相關的 command 以及對應的功能
 3. 實作相關的功能
 4. 測試相關的功能

# 通訊想要做到的功能
1. 進入即時通訊REPL 模式
2. 在REPL 模式下可以輸入 command 來對電路板做各種互動(測試或者功能切換)
3. 退出REPL 模式

# 功能
1. 電路板的測試細項
2. 各種 Sensor  的連續感測 , 連續輸出  , 未來可以在 PC 做出一個 感測器的即時示波器
3. 讀/寫 Data Flash，實現遠端參數設定，一個韌體多產品應用

---

# 通訊規格 v2（已實作）

## 1. 協議隔離原則
- 測試模式命令使用專用前綴：`AT+TEST,`
- 非 `AT+TEST,` 開頭資料，仍由原本遊戲 BLE 解析流程處理（不影響既有 `get cycle` / `set end` 等命令）。
- 所有回應以 `\r\n` 結尾。

## 2. 回應格式總覽

| 類別 | 格式 | 說明 |
|------|------|------|
| 成功 | `+OK,<CMD>[,<PAYLOAD>]\r\n` | CMD 對應送出的命令名 |
| 錯誤 | `+ERR,<CODE>,<MSG>\r\n` | CODE 見統一錯誤碼表 |
| 串流 | `+DATA,<SOURCE>,<...>\r\n` | 週期性主動推送 |

### 2.1 `+OK` 回應
```
+OK,<CMD>\r\n                    ← 純成功，無 payload
+OK,<CMD>,<PAYLOAD>\r\n          ← 帶 payload
+OK,<CMD>,<K1>=<V1>,<K2>=<V2>\r\n  ← 帶多組 key=value
```

### 2.2 `+ERR` 回應
```
+ERR,<CODE>,<MSG>\r\n
```

### 2.3 `+DATA` 串流
```
+DATA,<SOURCE>,<field1>,<field2>,...\r\n
```

## 3. 統一錯誤碼表

| 錯誤碼 | 說明 | 常見觸發情境 |
|--------|------|-------------|
| `STATE` | 目前狀態不允許此操作 | 未進入 REPL 就下指令；BLE 未連線 |
| `PARAM` | 參數缺失或不合法 | 數值超出範圍、格式錯誤 |
| `CMD` | 未知命令 | 打錯命令名、不存在的子命令 |
| `BUSY` | 資源忙碌 | （保留：未來用於互斥操作） |
| `FLASH` | Data Flash 操作失敗 | 寫入/擦除返回錯誤 |
| `RANGE` | 位址或長度超出 Data Flash 範圍 | offset + count 超出 DFLASH_SIZE |
| `ALIGN` | 位址未對齊 4 bytes | offset 不是 4 的倍數 |

PC 端 Parser 建議：先取 `+ERR,` 後的第一個逗號前字串作為 error code，再取第二個逗號後字串作為 detail。

## 4. 模式切換

| 命令 | 回應 |
|------|------|
| `AT+TEST,REPL_START` | `+OK,REPL_START,READY` |
| `AT+TEST,REPL_STOP`  | `+OK,REPL_STOP,BYE` |

> 除 `REPL_START / PING / VERSION / HELP / STATUS / STATUS_VERBOSE / REPL_STATE` 與 `SENSOR_STREAM` 系列外，其餘測試命令需在 REPL 模式下執行。

> 特例：`SENSOR_STREAM,START` 支援在 REPL 外直接呼叫，韌體會自動切入 REPL 並啟動串流。

> 特例：`SENSOR_STREAM,STOP` / `SENSOR_STREAM,QUERY` 支援在 REPL 外呼叫（idempotent）。

## 5. 完整命令列表

### 5.0 連線 / 查詢 / 診斷

#### `AT+TEST,PING`
連線存活探測（heartbeat），任何狀態皆可呼叫。

| 方向 | 內容 |
|------|------|
| TX → | `AT+TEST,PING` |
| ← RX | `+OK,PING,PONG` |

#### `AT+TEST,VERSION`
回傳韌體版本與編譯時間。

| 方向 | 內容 |
|------|------|
| TX → | `AT+TEST,VERSION` |
| ← RX | `+OK,VERSION,<major>.<minor>.<patch>,<date>,<time>` |

範例：
```
TX: AT+TEST,VERSION
RX: +OK,VERSION,1.0.0,Mar  7 2026,14:30:00
```

PC Parser：以逗號分割，`[1]`=版本, `[2]`=日期, `[3]`=時間。

#### `AT+TEST,HELP`
列出所有可用命令（以 `|` 分隔）。

| 方向 | 內容 |
|------|------|
| TX → | `AT+TEST,HELP` |
| ← RX | `+OK,HELP,PING\|VERSION\|REPL_START\|...` |

#### `AT+TEST,REPL_STATE`
回傳 REPL 啟用狀態與串流設定。

| 方向 | 內容 |
|------|------|
| TX → | `AT+TEST,REPL_STATE` |
| ← RX | `+OK,REPL_STATE,ACTIVE=<0\|1>,STREAM_EN=<0\|1>,SRC=<name>,INT=<ms>` |

範例：
```
TX: AT+TEST,REPL_STATE
RX: +OK,REPL_STATE,ACTIVE=1,STREAM_EN=1,SRC=GSENSOR,INT=100
```

PC Parser：以逗號分割並解析 key=value pairs。

#### `AT+TEST,STATUS`
回傳核心系統狀態。

| 方向 | 內容 |
|------|------|
| TX → | `AT+TEST,STATUS` |
| ← RX | `+OK,STATUS,BLE=<0\|1>,GAME=<0\|1>,REPL=<0\|1>,IDLE=<0\|1>,JUMP=<n>` |

範例：
```
TX: AT+TEST,STATUS
RX: +OK,STATUS,BLE=1,GAME=0,REPL=1,IDLE=0,JUMP=42
```

#### `AT+TEST,STATUS_VERBOSE`
回傳完整系統快照（兩行回應）。

| 方向 | 內容 |
|------|------|
| TX → | `AT+TEST,STATUS_VERBOSE` |
| ← RX Line1 | `+OK,STATUS_VERBOSE,BLE=<n>,GAME=<n>,REPL=<n>,IDLE=<n>,JUMP=<n>,KEY=<n>,PB7=<n>,PB8=<n>,ADC=<raw>,VBAT=<volt>` |
| ← RX Line2 | `+OK,STATUS_VERBOSE_G,AX=<x>,AY=<y>,AZ=<z>` |

範例：
```
TX: AT+TEST,STATUS_VERBOSE
RX: +OK,STATUS_VERBOSE,BLE=1,GAME=0,REPL=1,IDLE=0,JUMP=0,KEY=0,PB7=0,PB8=0,ADC=2048,VBAT=3.700
RX: +OK,STATUS_VERBOSE_G,AX=123,AY=-456,AZ=8192
```

PC Parser 注意：此命令回傳**兩行**，需等收滿兩行再 parse。收到 `STATUS_VERBOSE_G` 表示完整。

### 5.1 LED 控制

#### `AT+TEST,LED_ON`
| TX → | `AT+TEST,LED_ON` |
|------|------|
| ← RX | `+OK,LED_ON,1` |

#### `AT+TEST,LED_OFF`
| TX → | `AT+TEST,LED_OFF` |
|------|------|
| ← RX | `+OK,LED_OFF,0` |

#### `AT+TEST,LED_BLINK[,freq_hz[,duty]]`
| TX → | `AT+TEST,LED_BLINK` 或 `AT+TEST,LED_BLINK,5` 或 `AT+TEST,LED_BLINK,5,0.3` |
|------|------|
| ← RX | `+OK,LED_BLINK,APPLIED` |
| ← RX (error) | `+ERR,PARAM,LED_BLINK_FREQ` |

範例：
```
TX: AT+TEST,LED_BLINK,10,0.5
RX: +OK,LED_BLINK,APPLIED

TX: AT+TEST,LED_BLINK,0
RX: +ERR,PARAM,LED_BLINK_FREQ
```

### 5.2 Buzzer 控制

#### `AT+TEST,BUZZER_ON`
| TX → | `AT+TEST,BUZZER_ON` |
|------|------|
| ← RX | `+OK,BUZZER_ON,1000` |

payload 為預設頻率 Hz。

#### `AT+TEST,BUZZER_OFF`
| TX → | `AT+TEST,BUZZER_OFF` |
|------|------|
| ← RX | `+OK,BUZZER_OFF,0` |

#### `AT+TEST,BUZZER_BEEP[,freq_hz[,duration_ms]]`
| TX → | `AT+TEST,BUZZER_BEEP` 或 `AT+TEST,BUZZER_BEEP,2000,300` |
|------|------|
| ← RX | `+OK,BUZZER_BEEP,DONE` |
| ← RX (error) | `+ERR,PARAM,BUZZER_BEEP` |

範例：
```
TX: AT+TEST,BUZZER_BEEP,2000,300
RX: +OK,BUZZER_BEEP,DONE

TX: AT+TEST,BUZZER_BEEP,0,0
RX: +ERR,PARAM,BUZZER_BEEP
```

### 5.3 感測器讀取

#### `AT+TEST,SENSOR_READ`（G-Sensor 單次）
| TX → | `AT+TEST,SENSOR_READ` |
|------|------|
| ← RX | `+OK,SENSOR_READ,<ax>,<ay>,<az>` |

範例：
```
TX: AT+TEST,SENSOR_READ
RX: +OK,SENSOR_READ,123,-456,8192
```

PC Parser：`[1]`=AX, `[2]`=AY, `[3]`=AZ（int16 有號數）。

#### `AT+TEST,ADC_READ`（電池電壓）
| TX → | `AT+TEST,ADC_READ` |
|------|------|
| ← RX | `+OK,ADC_READ,<raw_u16>,<vbat_float>` |

範例：
```
TX: AT+TEST,ADC_READ
RX: +OK,ADC_READ,2048,3.700
```

PC Parser：`[1]`=raw ADC value (uint16), `[2]`=電壓 (float, 單位 V)。

#### `AT+TEST,HALL_READ`（Hall sensor）
| TX → | `AT+TEST,HALL_READ` |
|------|------|
| ← RX | `+OK,HALL_READ,PB7=<0\|1>,PB8=<0\|1>` |

範例：
```
TX: AT+TEST,HALL_READ
RX: +OK,HALL_READ,PB7=0,PB8=1
```

值為 1 表示 low-active 觸發。

#### `AT+TEST,KEY_READ`（按鈕）
| TX → | `AT+TEST,KEY_READ` |
|------|------|
| ← RX | `+OK,KEY_READ,PB15=<0\|1>` |

範例：
```
TX: AT+TEST,KEY_READ
RX: +OK,KEY_READ,PB15=1
```

值為 1 表示按鈕被按下（low-active）。

### 5.4 感測器串流

#### `AT+TEST,SENSOR_STREAM,START[,source[,interval_ms]]`

| 參數 | 預設 | 範圍 |
|------|------|------|
| `source` | `GSENSOR` | `GSENSOR` / `ADC` / `HALL` / `KEY` |
| `interval_ms` | `200` | `50` ~ `2000` |

| TX → | 範例 |
|------|------|
| 最簡 | `AT+TEST,SENSOR_STREAM,START` |
| 指定來源 | `AT+TEST,SENSOR_STREAM,START,ADC` |
| 指定來源+間隔 | `AT+TEST,SENSOR_STREAM,START,GSENSOR,100` |
| 僅指定間隔 | `AT+TEST,SENSOR_STREAM,START,500` |

回應：
```
+OK,SENSOR_STREAM,START,<source>,<actual_interval>
```

範例：
```
TX: AT+TEST,SENSOR_STREAM,START,GSENSOR,100
RX: +OK,SENSOR_STREAM,START,GSENSOR,100

TX: AT+TEST,SENSOR_STREAM,START,ADC,500
RX: +OK,SENSOR_STREAM,START,ADC,500

TX: AT+TEST,SENSOR_STREAM,START,30
RX: +OK,SENSOR_STREAM,START,GSENSOR,50     ← clamped to min 50ms
```

啟動後，韌體會依照 interval 持續推送 `+DATA` 串流：

| Source | 串流格式 | 範例 |
|--------|---------|------|
| `GSENSOR` | `+DATA,GSENSOR,<ax>,<ay>,<az>` | `+DATA,GSENSOR,123,-456,8192` |
| `ADC` | `+DATA,ADC,<raw>,<vbat>` | `+DATA,ADC,2048,3.700` |
| `HALL` | `+DATA,HALL,PB7=<n>,PB8=<n>` | `+DATA,HALL,PB7=0,PB8=1` |
| `KEY` | `+DATA,KEY,PB15=<n>` | `+DATA,KEY,PB15=0` |

#### `AT+TEST,SENSOR_STREAM,STOP`
```
TX: AT+TEST,SENSOR_STREAM,STOP
RX: +OK,SENSOR_STREAM,STOP
```

#### `AT+TEST,SENSOR_STREAM,QUERY`
```
TX: AT+TEST,SENSOR_STREAM,QUERY
RX: +OK,SENSOR_STREAM,QUERY,EN=1,SRC=GSENSOR,INT=100
```

PC Parser：以逗號分割，解析 `EN` / `SRC` / `INT`。

### 5.5 Data Flash 操作

Data Flash 用於保存持久性參數（FW 版本、遊戲模式、設定值等），實現**一個韌體、多產品應用**的遠端設定能力。

- 預設使用 APROM 最後一頁 (4 KB, base = `0x7F000`)
- 所有 offset 以 hex 表示，必須 4-byte 對齊
- 單次讀取最多 64 words（256 bytes）
- 寫入為 word-level（4 bytes），需先自行 erase 再寫

#### `AT+TEST,DFLASH_INFO`
查詢 Data Flash 配置。

```
TX: AT+TEST,DFLASH_INFO
RX: +OK,DFLASH_INFO,BASE=0x0007F000,SIZE=0x1000,PAGE=0x1000
```

PC Parser：`BASE`=起始位址, `SIZE`=可用大小 bytes, `PAGE`=page 大小 bytes。

#### `AT+TEST,DFLASH_READ,<hex_offset>[,<word_count>]`
讀取 Data Flash 內容（word_count 預設 1，最大 64）。每個 word 一行回應。

```
TX: AT+TEST,DFLASH_READ,0000
RX: +OK,DFLASH_READ,0x0000,0xFFFFFFFF

TX: AT+TEST,DFLASH_READ,0000,4
RX: +OK,DFLASH_READ,0x0000,0xFFFFFFFF
RX: +OK,DFLASH_READ,0x0004,0xFFFFFFFF
RX: +OK,DFLASH_READ,0x0008,0xFFFFFFFF
RX: +OK,DFLASH_READ,0x000C,0xFFFFFFFF
```

回應格式：`+OK,DFLASH_READ,<hex_offset>,<hex_value>`

PC Parser：每行 `[1]`=offset, `[2]`=value。讀 N 個 word 會收到 N 行。

錯誤範例：
```
TX: AT+TEST,DFLASH_READ,0003
RX: +ERR,ALIGN,DFLASH_READ_OFFSET

TX: AT+TEST,DFLASH_READ,2000
RX: +ERR,RANGE,DFLASH_READ_END
```

#### `AT+TEST,DFLASH_WRITE,<hex_offset>,<hex_value>`
寫入單一 word 到 Data Flash。

```
TX: AT+TEST,DFLASH_WRITE,0000,12345678
RX: +OK,DFLASH_WRITE,0x0000,0x12345678

TX: AT+TEST,DFLASH_WRITE,0004,AABBCCDD
RX: +OK,DFLASH_WRITE,0x0004,0xAABBCCDD
```

回應格式：`+OK,DFLASH_WRITE,<hex_offset>,<hex_value>`

> ⚠️ Flash 寫入規則：只能將 bit 從 1→0。若目標位址已有非 0xFF 的值，需先 ERASE 整頁再寫入。

錯誤範例：
```
TX: AT+TEST,DFLASH_WRITE,0003,12345678
RX: +ERR,ALIGN,DFLASH_WRITE_OFFSET

TX: AT+TEST,DFLASH_WRITE
RX: +ERR,PARAM,DFLASH_WRITE_PARAMS
```

#### `AT+TEST,DFLASH_ERASE`
擦除整頁 Data Flash（全部變 0xFFFFFFFF）。

```
TX: AT+TEST,DFLASH_ERASE
RX: +OK,DFLASH_ERASE,DONE
```

> ⚠️ 此操作不可逆，會清除所有 Data Flash 內容。

### 5.6 Data Flash 應用規劃（建議 layout）

以下為建議的 Data Flash 分配表，供 PC 工具與韌體共同遵守：

| Offset | 大小 (bytes) | 用途 | 值範例 |
|--------|-------------|------|--------|
| `0x000` | 4 | Magic number (`0x524C5350` = "RLSP") | `0x524C5350` |
| `0x004` | 4 | Layout version | `0x00000001` |
| `0x008` | 4 | Product ID（產品型號區分） | `0x00000001` |
| `0x00C` | 4 | Game mode（遊戲模式） | `0x00000000` |
| `0x010` | 4 | Buzzer volume (0-10) | `0x00000005` |
| `0x014` | 4 | LED brightness (0-100) | `0x00000064` |
| `0x018` | 4 | G-Sensor sensitivity | `0x00000003` |
| `0x01C` | 4 | Idle timeout (seconds) | `0x0000003C` |
| `0x020` | 4 | BLE TX power level | `0x00000004` |
| `0x024` ~ `0x03F` | 28 | 保留 | `0xFFFFFFFF` |
| `0x040` ~ `0x07F` | 64 | 使用者自訂名稱 (16 chars) | ASCII string |
| `0x080` ~ `0xFFF` | 3968 | 保留（未來擴充） | `0xFFFFFFFF` |

PC 工具可透過 `DFLASH_READ` / `DFLASH_WRITE` / `DFLASH_ERASE` 完成遠端設定：

```
# 讀取產品 ID
TX: AT+TEST,DFLASH_READ,0008
RX: +OK,DFLASH_READ,0x0008,0x00000001

# 更改遊戲模式為 2
TX: AT+TEST,DFLASH_ERASE          ← 先擦除整頁
RX: +OK,DFLASH_ERASE,DONE
TX: AT+TEST,DFLASH_WRITE,0000,524C5350  ← 寫回 magic
RX: +OK,DFLASH_WRITE,0x0000,0x524C5350
TX: AT+TEST,DFLASH_WRITE,000C,00000002  ← 新遊戲模式
RX: +OK,DFLASH_WRITE,0x000C,0x00000002
```

---

## 6. PC 端 Parser 快速指南

### 6.1 通用解析流程
```
1. 收到一行 (以 \r\n 結尾)
2. 判斷前綴：
   - "+OK,"   → 成功回應
   - "+ERR,"  → 錯誤回應
   - "+DATA," → 串流資料
3. 以逗號分割成 tokens[]
4. tokens[0] = 前綴 (+OK / +ERR / +DATA)
5. tokens[1] = 命令名 / 錯誤碼 / 串流來源
6. tokens[2..N] = payload fields
```

### 6.2 各命令 payload 欄位速查表

| 命令 | tokens[1] | tokens[2] | tokens[3] | tokens[4] | tokens[5] |
|------|-----------|-----------|-----------|-----------|-----------|
| PING | `PING` | `PONG` | — | — | — |
| VERSION | `VERSION` | `M.m.p` | `date` | `time` | — |
| REPL_START | `REPL_START` | `READY` | — | — | — |
| REPL_STOP | `REPL_STOP` | `BYE` | — | — | — |
| REPL_STATE | `REPL_STATE` | `ACTIVE=n` | `STREAM_EN=n` | `SRC=name` | `INT=ms` |
| STATUS | `STATUS` | `BLE=n` | `GAME=n` | `REPL=n` | `IDLE=n`... |
| LED_ON | `LED_ON` | `1` | — | — | — |
| LED_OFF | `LED_OFF` | `0` | — | — | — |
| LED_BLINK | `LED_BLINK` | `APPLIED` | — | — | — |
| BUZZER_ON | `BUZZER_ON` | `1000` | — | — | — |
| BUZZER_OFF | `BUZZER_OFF` | `0` | — | — | — |
| BUZZER_BEEP | `BUZZER_BEEP` | `DONE` | — | — | — |
| SENSOR_READ | `SENSOR_READ` | `ax` | `ay` | `az` | — |
| ADC_READ | `ADC_READ` | `raw` | `vbat` | — | — |
| HALL_READ | `HALL_READ` | `PB7=n` | `PB8=n` | — | — |
| KEY_READ | `KEY_READ` | `PB15=n` | — | — | — |
| SENSOR_STREAM START | `SENSOR_STREAM` | `START` | `source` | `interval` | — |
| SENSOR_STREAM STOP | `SENSOR_STREAM` | `STOP` | — | — | — |
| SENSOR_STREAM QUERY | `SENSOR_STREAM` | `QUERY` | `EN=n` | `SRC=name` | `INT=ms` |
| DFLASH_INFO | `DFLASH_INFO` | `BASE=0x...` | `SIZE=0x...` | `PAGE=0x...` | — |
| DFLASH_READ | `DFLASH_READ` | `0xOOOO` | `0xVVVVVVVV` | — | — |
| DFLASH_WRITE | `DFLASH_WRITE` | `0xOOOO` | `0xVVVVVVVV` | — | — |
| DFLASH_ERASE | `DFLASH_ERASE` | `DONE` | — | — | — |

### 6.3 串流 `+DATA` 欄位速查表

| tokens[1] (SOURCE) | tokens[2] | tokens[3] | tokens[4] |
|---------------------|-----------|-----------|-----------|
| `GSENSOR` | `ax` (int16) | `ay` (int16) | `az` (int16) |
| `ADC` | `raw` (uint16) | `vbat` (float) | — |
| `HALL` | `PB7=n` | `PB8=n` | — |
| `KEY` | `PB15=n` | — | — |

### 6.4 錯誤回應解析
```
+ERR,STATE,NOT_IN_REPL       → code="STATE", detail="NOT_IN_REPL"
+ERR,PARAM,LED_BLINK_FREQ    → code="PARAM", detail="LED_BLINK_FREQ"
+ERR,CMD,UNKNOWN              → code="CMD",   detail="UNKNOWN"
+ERR,FLASH,DFLASH_WRITE_FAIL → code="FLASH", detail="DFLASH_WRITE_FAIL"
+ERR,RANGE,DFLASH_READ_END   → code="RANGE", detail="DFLASH_READ_END"
+ERR,ALIGN,DFLASH_READ_OFFSET→ code="ALIGN", detail="DFLASH_READ_OFFSET"
```

## 7. 串流保護
- `SENSOR_STREAM` interval 會限制在 `50ms ~ 2000ms`，避免 UART 負載過高。
- BLE 斷線時自動停止串流並退出 REPL 模式。

## 8. 完整互動範例

```
# 1. 探測連線
TX: AT+TEST,PING
RX: +OK,PING,PONG

# 2. 查看版本
TX: AT+TEST,VERSION
RX: +OK,VERSION,1.0.0,Mar  7 2026,14:30:00

# 3. 進入 REPL
TX: AT+TEST,REPL_START
RX: +OK,REPL_START,READY

# 4. 點亮 LED + 發出 beep
TX: AT+TEST,LED_ON
RX: +OK,LED_ON,1
TX: AT+TEST,BUZZER_BEEP,2000,200
RX: +OK,BUZZER_BEEP,DONE

# 5. 讀取所有感測器
TX: AT+TEST,STATUS_VERBOSE
RX: +OK,STATUS_VERBOSE,BLE=1,GAME=0,REPL=1,IDLE=0,JUMP=0,KEY=0,PB7=0,PB8=0,ADC=2048,VBAT=3.700
RX: +OK,STATUS_VERBOSE_G,AX=123,AY=-456,AZ=8192

# 6. 啟動 G-Sensor 串流
TX: AT+TEST,SENSOR_STREAM,START,GSENSOR,100
RX: +OK,SENSOR_STREAM,START,GSENSOR,100
RX: +DATA,GSENSOR,120,-440,8200
RX: +DATA,GSENSOR,125,-435,8195
RX: +DATA,GSENSOR,118,-450,8210
...

# 7. 停止串流
TX: AT+TEST,SENSOR_STREAM,STOP
RX: +OK,SENSOR_STREAM,STOP

# 8. 讀取 Data Flash
TX: AT+TEST,DFLASH_INFO
RX: +OK,DFLASH_INFO,BASE=0x0007F000,SIZE=0x1000,PAGE=0x1000
TX: AT+TEST,DFLASH_READ,0000,4
RX: +OK,DFLASH_READ,0x0000,0xFFFFFFFF
RX: +OK,DFLASH_READ,0x0004,0xFFFFFFFF
RX: +OK,DFLASH_READ,0x0008,0xFFFFFFFF
RX: +OK,DFLASH_READ,0x000C,0xFFFFFFFF

# 9. 寫入 Data Flash
TX: AT+TEST,DFLASH_WRITE,0000,524C5350
RX: +OK,DFLASH_WRITE,0x0000,0x524C5350

# 10. 離開 REPL
TX: AT+TEST,REPL_STOP
RX: +OK,REPL_STOP,BYE
```


