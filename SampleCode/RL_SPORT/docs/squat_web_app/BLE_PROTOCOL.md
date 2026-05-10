# SQUAT Web ↔ BLE 通訊規格（AT+TEST）

此規格給 `docs/squat_web_app/index.html` 與 M480 韌體使用。

## 1) 連線

- GATT Service UUID: `524cacc0-3c17-d293-8e48-14fe2e4da212`
- RX(網頁寫入到裝置) Characteristic UUID: `0000d002-0000-1000-8000-00805f9b34fb`
- TX(裝置通知到網頁) Characteristic UUID: `0000d001-0000-1000-8000-00805f9b34fb`

> 實作上以 UTF-8 純文字傳送，每筆命令以 `\r\n` 結尾。

---

## 2) Web -> 韌體（命令）

### 2.1 開始並歸零

- `AT+TEST,SQUAT_START`
- `AT+TEST,SQUAT_RESET`

建議流程：
1. 先送 `SQUAT_START`
2. 再送 `SQUAT_RESET`

---

### 2.2 設定 8x8 顯示（新增）

- `AT+TEST,SQUAT_LED,SET,<COUNT>,<PHASE>,<PROG>`
  - `COUNT`: `0~99`
  - `PHASE`: `IDLE|STAND|DESCEND|BOTTOM|ASCEND`
  - `PROG`: `0~8`

範例：
- `AT+TEST,SQUAT_LED,SET,12,ASCEND,6`

其他控制：
- `AT+TEST,SQUAT_LED,CLEAR`：取消遠端覆寫，回到韌體本地顯示
- `AT+TEST,SQUAT_LED,QUERY`：查詢遠端覆寫是否啟用

---

### 2.3 狀態串流

- `AT+TEST,SQUAT_STREAM,START,STATE,200`
- `AT+TEST,SQUAT_STREAM,STOP`

---

## 3) 韌體 -> Web（回應）

### 3.1 一般回應

- `+OK,SQUAT_START,OK`
- `+OK,SQUAT_RESET,OK`
- `+OK,SQUAT_LED,SET,COUNT=12,PHASE=ASCEND,PROG=6`
- `+OK,SQUAT_LED,CLEAR`
- `+OK,SQUAT_LED,QUERY,EN=1`

### 3.2 狀態資料

- `+DATA,SQUAT_STATE,STATE=STAND,COUNT=5,PROG=2`

Web 端應解析 `STATE` / `COUNT` / `PROG` 後更新畫面。

---

## 4) 設計目的

1. 由網頁控制開始/歸零。
2. 由網頁下發 8x8 要顯示的內容（COUNT/PHASE/PROG）。
3. 由韌體回傳狀態，網頁顯示「狀態 + 次數」。
