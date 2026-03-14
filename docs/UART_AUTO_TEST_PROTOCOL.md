# RL_SPORT V3 — UART 自動測試通訊協議

## 1. 概述

本協議定義 RL_SPORT V3 硬體板透過 **UART0** 接受外部自動測試工具的命令，逐項檢測每個硬體模組，並回傳結構化的 PASS/FAIL 結果。

### 設計目標
- 可被 Python / C# 等自動化工具程式化驅動
- 每條命令有明確的結構化回應，方便解析
- 與原本的互動式 test menu（輸入 `test`）共存
- 涵蓋板上所有可測試硬體

### 物理介面
| 參數 | 值 |
|------|----|
| UART | UART0 (Debug Port) |
| Baud Rate | 115200 |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Flow Control | None |
| Line Ending | `\r\n` (CR+LF) |

---

## 2. 協議格式

### 2.1 命令格式 (Host → MCU)

```
AT+TEST=<CMD>[,<PARAM1>][,<PARAM2>]\r\n
```

- 命令以 `AT+TEST=` 為前綴
- CMD 為大寫英文命令名稱
- 參數以逗號分隔
- 以 `\r\n` 結尾

### 2.2 回應格式 (MCU → Host)

**成功：**
```
+TEST:<CMD>,PASS[,<KEY1>=<VAL1>][,<KEY2>=<VAL2>]\r\n
OK\r\n
```

**失敗：**
```
+TEST:<CMD>,FAIL,<REASON>\r\n
ERROR\r\n
```

**多行回應（例如 I2C SCAN）：**
```
+TEST:<CMD>,<DATA_LINE>\r\n
+TEST:<CMD>,<DATA_LINE>\r\n
+TEST:<CMD>,PASS[,SUMMARY]\r\n
OK\r\n
```

### 2.3 超時

- Host 應在發送命令後等待回應，建議通用超時 **5 秒**
- KEY 和 HALL,WAIT 命令有自訂 timeout 參數

### 2.4 錯誤碼

| 回應 | 說明 |
|------|------|
| `OK` | 命令執行成功 |
| `ERROR` | 命令執行失敗或參數錯誤 |
| `+TEST:UNKNOWN,FAIL,BAD_CMD` | 無法識別的命令 |

---

## 3. 命令清單

### 3.1 系統資訊

#### `AT+TEST=INFO`
查詢韌體版本與板號。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=INFO\r\n` |
| MCU → Host | `+TEST:INFO,PASS,FW=x.y.z,BRD=RL_SPORT_V3,BUILD=Mmm_dd_yyyy_HH:MM:SS\r\n` |
|            | `OK\r\n` |

> **BUILD** 欄位為編譯時 `__DATE__` 與 `__TIME__` 組合，格式如 `Jun_10_2025_14:30:00`。

---

### 3.2 LED 測試

#### `AT+TEST=LED,ON`
開啟 LED (PB3)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=LED,ON\r\n` |
| MCU → Host | `+TEST:LED,PASS,STATE=ON\r\n` |
|            | `OK\r\n` |

#### `AT+TEST=LED,OFF`
關閉 LED (PB3)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=LED,OFF\r\n` |
| MCU → Host | `+TEST:LED,PASS,STATE=OFF\r\n` |
|            | `OK\r\n` |

#### `AT+TEST=LED,BLINK`
LED 閃爍 3 次。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=LED,BLINK\r\n` |
| MCU → Host | `+TEST:LED,PASS,BLINK=3\r\n` |
|            | `OK\r\n` |

---

### 3.3 蜂鳴器測試

#### `AT+TEST=BUZZER[,<freq_hz>,<duration_ms>]`
驅動蜂鳴器 (PC7)。不帶參數時使用預設值 1000Hz / 200ms。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=BUZZER\r\n` |
| MCU → Host | `+TEST:BUZZER,PASS,FREQ=1000,DUR=200\r\n` |
|            | `OK\r\n` |

帶參數範例：
| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=BUZZER,2000,500\r\n` |
| MCU → Host | `+TEST:BUZZER,PASS,FREQ=2000,DUR=500\r\n` |
|            | `OK\r\n` |

---

### 3.4 按鍵測試

#### `AT+TEST=KEY[,<timeout_ms>]`
等待按鍵 PB15 按下。不帶參數時預設 5000ms。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=KEY,3000\r\n` |
| MCU → Host (成功) | `+TEST:KEY,PASS,T=1234\r\n` |
|                   | `OK\r\n` |
| MCU → Host (超時) | `+TEST:KEY,FAIL,TIMEOUT\r\n` |
|                   | `ERROR\r\n` |

`T=` 表示按鍵被偵測到時經過的毫秒數。

---

### 3.5 Hall 感測器測試

#### `AT+TEST=HALL`
立即讀取 Hall 感測器腳位狀態 (PB7/PB8)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=HALL\r\n` |
| MCU → Host | `+TEST:HALL,PASS,PB7=1,PB8=0\r\n` |
|            | `OK\r\n` |

#### `AT+TEST=HALL,WAIT,<timeout_ms>`
等待 Hall 邊緣變化。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=HALL,WAIT,3000\r\n` |
| MCU → Host (成功) | `+TEST:HALL,PASS,EDGE=PB7,T=450\r\n` |
|                   | `OK\r\n` |
| MCU → Host (超時) | `+TEST:HALL,FAIL,TIMEOUT\r\n` |
|                   | `ERROR\r\n` |

---

### 3.6 加速度計 (G-Sensor) 測試

#### `AT+TEST=GSENSOR`
讀取 G-Sensor (MXC400, I2C addr 0x15) 三軸數值，並同時輸出原始合力與校正後合力。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=GSENSOR\r\n` |
| MCU → Host (成功) | `+TEST:GSENSOR,PASS,X=12,Y=-8,Z=1020,G_RAW=1.281,G_CAL=1.002,CAL=1\r\n` |
|            | `OK\r\n` |
| MCU → Host (失敗) | `+TEST:GSENSOR,FAIL,G_RAW=0.123,G_CAL=0.123,CAL=0,X=0,Y=0,Z=0\r\n` |
|            | `ERROR\r\n` |

`CAL` 欄位：`1` 表示已有執行過 `AT+TEST=GSENSOR,CAL`；`0` 表示尚未校正（此時 `G_CAL == G_RAW`）。

#### `AT+TEST=GSENSOR,CAL`
在裝置靜止且板面朝向不變的情況下進行自動校正，以取得比例 (scale)，供後續 `AT+TEST=GSENSOR` 計算 `G_CAL` 使用。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=GSENSOR,CAL\r\n` |
| MCU → Host (成功) | `+TEST:GSENSOR,PASS,CAL,SAMPLES=64,G_AVG=1.001,SCALE=0.999000\r\n` |
|            | `OK\r\n` |
| MCU → Host (失敗) | `+TEST:GSENSOR,FAIL,CAL,NO_VALID_DATA\r\n` |
|            | `ERROR\r\n` |

**行為說明與判定標準**
- 校正採取多次取樣並計算合力（magnitude）的平均值，實作上目前樣本數為 64（`TEST_GSENSOR_CAL_SAMPLES`）。
- 計算方式：SCALE = 1.0 / G_AVG，之後 `G_CAL = G_RAW * SCALE`。
- `AT+TEST=GSENSOR` 會同時回傳下列欄位：
      - `G_RAW`：依照 sensor 原始 raw counts 換算的合力 (g)
      - `G_CAL`：套用 SCALE 後的合力 (g)
      - `G_USE`：實際用來進行 PASS/FAIL 判定的值（firmware 會比較 RAW 與 CAL 哪個更接近 1g，並使用較佳者）
      - `SRC`：`RAW` 或 `CAL`，指出 `G_USE` 來源
      - `CAL`：表示是否已有有效校正 (1 = 已校正，0 = 未校正)

- 判定通過：以 `G_USE` 是否落在 [1.0 - MOVEMENT_MAG_TOLERANCE_G, 1.0 + MOVEMENT_MAG_TOLERANCE_G] 為準（預設 `MOVEMENT_MAG_TOLERANCE_G = 0.7`，即通過區間 [0.3, 1.7]）。
- 若 I2C 讀取失敗或回傳全 0，將回報 FAIL（`+TEST:GSENSOR,FAIL,...`）。

**重要：sensor warm-up 與一致性保證**
- 在 `AT+TEST=ALL` 的整合流程中，LED 閃爍與 Buzzer 響聲會提供數百毫秒到 ~850ms 的 I2C 閒置/穩定時間，這使得後續的 GSENSOR 讀值穩定。
- 為了讓單獨執行 `AT+TEST=GSENSOR` 時也能取得一致結果，firmware 在單次 GSENSOR 測項中會執行以下步驟：
      1. 呼叫 `GsensorWakeup()` 重新啟動 sensor 的測量管線
      2. 等待 100 ms 以讓 sensor 完成若干轉換週期
      3. 丟棄 5 筆過渡樣本（每筆間隔 10 ms）以清除殘留資料
      4. 讀取並平均 8 筆樣本（每筆間隔 10 ms）作為最終輸出

這個行為確保 `AT+TEST=GSENSOR` 的結果在單項與 `AT+TEST=ALL` 中能夠一致。

**硬體換算說明**
- 在 MXC4005XC 實機上已驗證：FSR=2G 時實際觀測約為 2048 counts/g（firmware 中 `s_fsr_cpg[FSR_2G] = 2048.0f`），因此文件中的換算以 2048 counts/g 為準；`CAL` 會再進一步修正小量偏移。

***

---

### 3.7 ADC 電池電壓測試

#### `AT+TEST=ADC`
讀取電池電壓 (PB1, EADC0_CH1)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=ADC\r\n` |
| MCU → Host (成功) | `+TEST:ADC,PASS,RAW=2048,MV=3300\r\n` |
|                   | `OK\r\n` |
| MCU → Host (失敗) | `+TEST:ADC,FAIL,OUT_OF_RANGE\r\n` |
|                   | `ERROR\r\n` |

**判定標準：** 2.0V ≤ V ≤ 5.5V。`MV=` 為毫伏整數值。

---

### 3.8 I2C Bus 掃描

#### `AT+TEST=I2C,SCAN`
掃描 I2C0 bus (地址 0x08-0x77)，回報有回應的裝置。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=I2C,SCAN\r\n` |
| MCU → Host | `+TEST:I2C,FOUND,ADDR=0x15\r\n` |
|            | `+TEST:I2C,INFO,G=0.995\r\n` |
|            | `+TEST:I2C,PASS,COUNT=1\r\n` |
|            | `OK\r\n` |

無裝置時：
| 方向 | 內容 |
|------|------|
| MCU → Host | `+TEST:I2C,FAIL,NO_DEVICE\r\n` |
|            | `ERROR\r\n` |

---

### 3.9 BLE 模組測試

#### `AT+TEST=BLE,NAME`
查詢 BLE 模組名稱 (需回應 ROPR_ 或 ROPE_ 前綴)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=BLE,NAME\r\n` |
| MCU → Host (成功) | `+TEST:BLE,PASS,NAME=ROPR_1234\r\n` |
|                   | `OK\r\n` |
| MCU → Host (失敗) | `+TEST:BLE,FAIL,NO_RESPONSE\r\n` |
|                   | `ERROR\r\n` |

#### `AT+TEST=BLE,MAC`
查詢 BLE 模組 MAC 地址。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=BLE,MAC\r\n` |
| MCU → Host (成功) | `+TEST:BLE,PASS,MAC=AA:BB:CC:DD:EE:FF\r\n` |
|                   | `OK\r\n` |
| MCU → Host (失敗) | `+TEST:BLE,FAIL,NO_RESPONSE\r\n` |
|                   | `ERROR\r\n` |

註：在 `AT+TEST=ALL` 的整合流程中，BLE 的 MAC 查詢會被視為資訊性項目（fixture 可能不接 MAC 回傳），若無回應會回傳 `+TEST:BLE,INFO,MAC=NA` 而不視為整體 FAIL；但單獨執行 `AT+TEST=BLE,MAC` 時若無回應仍會回傳 FAIL/ERROR。

---

### 3.10 USB HID 測試

#### `AT+TEST=USB`
執行 USB FS HID Mouse 自動測試 (5 秒)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=USB\r\n` |
| MCU → Host | `+TEST:USB,PASS,DUR=5000\r\n` |
|            | `OK\r\n` |

---

### 3.11 電源管理測試

#### `AT+TEST=PWR,VBUS`
偵測 USB VBUS 充電狀態。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=PWR,VBUS\r\n` |
| MCU → Host | `+TEST:PWR,PASS,VBUS=1\r\n` |
|            | `OK\r\n` |

#### `AT+TEST=PWR,LOCK`
讀取 Power Lock 腳位 (PA11) 狀態。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=PWR,LOCK\r\n` |
| MCU → Host | `+TEST:PWR,PASS,PA11=1\r\n` |
|            | `OK\r\n` |

---

### 3.12 全項目測試

#### `AT+TEST=ALL`
依序執行所有自動判定測項 (不含需人工操作的 KEY 和 HALL,WAIT)。

| 方向 | 內容 |
|------|------|
| Host → MCU | `AT+TEST=ALL\r\n` |
| MCU → Host | 逐項回報 `+TEST:<CMD>,PASS/FAIL/INFO,...` |
|            | `+TEST:ALL,DONE,PASS=N,FAIL=M,TOTAL=T\r\n` |
|            | `OK\r\n` (全 PASS) 或 `ERROR\r\n` (有 FAIL) |

**ALL 執行順序：**
1. INFO
2. LED,BLINK
3. BUZZER
4. GSENSOR
5. ADC
6. I2C,SCAN
7. HALL (即時讀取)
8. BLE,NAME
9. BLE,MAC
10. PWR,VBUS
11. PWR,LOCK

---

## 4. 使用流程範例

### 4.1 自動化工具完整測試流程

```
Host                          MCU
 │                             │
 │  AT+TEST=INFO\r\n          │
 │ ──────────────────────────► │
 │                             │
 │  +TEST:INFO,PASS,...        │
 │  OK                         │
 │ ◄────────────────────────── │
 │                             │
 │  AT+TEST=ALL\r\n           │
 │ ──────────────────────────► │
 │                             │
 │  +TEST:LED,PASS,BLINK=3    │
 │  +TEST:BUZZER,PASS,...      │
 │  +TEST:GSENSOR,PASS,...     │
 │  +TEST:ADC,PASS,...         │
 │  +TEST:I2C,FOUND,...        │
 │  +TEST:I2C,PASS,...         │
 │  +TEST:HALL,PASS,...        │
 │  +TEST:BLE,PASS,...         │
 │  +TEST:BLE,PASS,...         │
 │  +TEST:PWR,PASS,...         │
 │  +TEST:PWR,PASS,...         │
 │  +TEST:ALL,DONE,PASS=11,...  │
 │  OK                         │
 │ ◄────────────────────────── │
```

### 4.2 單項互動式測試 (人工搭配治具)

```
Host: AT+TEST=KEY,5000\r\n   → 操作員按下按鍵
MCU:  +TEST:KEY,PASS,T=2100
      OK

Host: AT+TEST=HALL,WAIT,3000\r\n  → 操作員放置磁鐵
MCU:  +TEST:HALL,PASS,EDGE=PB7,T=800
      OK
```

---

## 5. 實作說明

### 5.1 進入方式
- AT 命令解析器在 **main loop** 中輪詢 UART0
- 不需要先輸入 `"test"` — 直接發送 `AT+TEST=` 命令即可觸發
- 原有的互動式 `"test"` 選單仍然保留

### 5.2 與原有 test menu 共存
- `TestMode_PollEnter()` 先檢查是否為 `AT+TEST=` 前綴
- 若是，則交由 AT 解析器處理
- 若否，則維持原本的 `"test"` 命令判斷邏輯

### 5.3 Source Files
| 檔案 | 變更 |
|------|------|
| `project_config.h` | FW_VERSION / BOARD_NAME / FW_BUILD_DATE / FW_BUILD_TIME 定義 |
| `test_mode.h` | TEST_FW_VERSION / TEST_BOARD_NAME → 引用 project_config.h |
| `test_mode.c` | AT 命令解析器與所有測試函式 |
| `main.c` | 在 main loop 中呼叫 `TestMode_PollEnter()` |

---

## 6. 版本紀錄

| 版本 | 日期 | 說明 |
|------|------|------|
| 1.3 | 2026-03-14 | 移除 `AT+TEST` 除錯回應，新增 `AT+TEST=GSENSOR,CAL`，`AT+TEST=GSENSOR` 回傳 `G_RAW`/`G_CAL` |
| 1.2 | 2025-06 | INFO 輸出加入 BUILD= 編譯日期時間，FW 版本定義移至 project_config.h |
| 1.1 | 2026-03-14 | 更新 — 將 FW 版本調整為 1.1.0，GSENSOR 與 I2C/I2C SCAN 行為與回傳欄位同步 |
