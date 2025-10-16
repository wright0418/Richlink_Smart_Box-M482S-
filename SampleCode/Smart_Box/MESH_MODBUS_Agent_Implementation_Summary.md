# MESH MODBUS Agent Mode 實作完成總結

## ✅ 已完成的項目

### 1. 新增檔案
- ✅ **mesh_modbus_agent.h** - Agent 模式的標頭檔
- ✅ **mesh_modbus_agent.c** - Agent 模式的實作檔

### 2. 修改檔案
- ✅ **mesh_handler.h** - 新增 `mesh_agent_response_callback_t` 回調定義
- ✅ **mesh_handler.c** - 整合 Agent 訊息識別與處理邏輯
- ✅ **main.c** - 初始化 Agent 並加入主迴圈輪詢
- ✅ **VSCode/Smart_Box.cproject.yml** - 加入 `mesh_modbus_agent.c` 到建置系統

---

## 📋 功能摘要

### **兩種運作模式**

#### **1. RL Device Mode（帶 Header）**
- **資料格式**：`header(2) + type(1) + MODBUS RTU package(8)`
  - Header: `0x82 0x76`
  - Type: `0x01` (GET) / `0x02` (SET) / `0x03` (RTU)
- **回應格式**：`header(2) + type(1) + MODBUS 回應資料（不含 CRC）`
- **錯誤處理**：CRC 錯誤時回傳 `82 76 <type> FE`，並閃紅燈兩次

#### **2. Bypass Mode（無 Header）**
- **資料格式**：`MODBUS RTU package(8)`
- **回應格式**：`MODBUS 回應資料（不含 CRC）`
- **錯誤處理**：CRC 錯誤時不回傳，僅閃紅燈兩次

---

## 🔧 核心實作

### **mesh_modbus_agent.c 主要功能**

1. **資料解析**
   - `parse_rl_mode_data()` - 解析 RL Mode 格式
   - `parse_bypass_mode_data()` - 解析 Bypass Mode 格式

2. **MODBUS 請求發送**
   - `send_modbus_request()` - 根據 function code 調用對應的 MODBUS Client API
   - 支援：Read Holding (0x03)、Read Input (0x04)、Write Single (0x06)、Write Multiple (0x10)

3. **回應處理**
   - `handle_modbus_response()` - 監控 MODBUS Client 狀態
   - 驗證 CRC（使用 `modbus_crc16_compute()`）
   - 超時檢測與錯誤處理

4. **回應組裝**
   - `prepare_mesh_response()` - 根據模式組裝回傳資料
   - 自動去除 CRC 後兩個位元組

---

## 🔄 資料流程

```
BLE Mesh (Hex String)
    ↓
mesh_handler_process_line() 
    [識別 Agent 格式]
    ↓
agent_mesh_data_callback() (main.c)
    ↓
mesh_modbus_agent_process_mesh_data()
    [解析格式 → 發送 MODBUS 請求]
    ↓
UART0 RS485 → MODBUS Device
    ↓
MODBUS Device 回應
    ↓
mesh_modbus_agent_poll()
    [檢查回應 → 驗證 CRC → 去除 CRC]
    ↓
agent_response_ready_callback() (main.c)
    [轉成 Hex String]
    ↓
ble_mesh_at_send_mdts()
    ↓
BLE Mesh (Hex String)
```

---

## 🎯 訊息識別邏輯（mesh_handler.c）

```c
// Agent 訊息識別條件：
1. RL Mode: 長度 >= 11 且開頭為 0x82 0x76
2. Bypass Mode: 長度 == 8 且符合 MODBUS 格式
   - slave_addr: 1-247
   - function_code: 0x03, 0x04, 0x06, 0x10
```

---

## ⚙️ 初始化配置（main.c）

```c
mesh_modbus_agent_config_t agent_config = {
    .mode = MESH_MODBUS_AGENT_MODE_RL,  // 可改為 BYPASS
    .modbus_timeout_ms = 500,            // MODBUS 超時
    .max_response_wait_ms = 1000         // 最大等待時間
};
```

---

## 🚨 錯誤處理

| 錯誤情況 | RL Mode 行為 | Bypass Mode 行為 |
|---------|-------------|-----------------|
| CRC 錯誤 | 回傳 `827601FE` + 閃紅燈2次 | 不回傳 + 閃紅燈2次 |
| MODBUS 超時 | 回傳 `827601FE` + 閃紅燈2次 | 不回傳 + 閃紅燈2次 |
| MODBUS 異常 | 回傳 `827601FE` + 閃紅燈2次 | 不回傳 + 閃紅燈2次 |

---

## 💡 LED 指示

| 事件 | LED 動作 |
|-----|---------|
| Agent 回應成功 | 藍燈脈衝 120ms |
| CRC 錯誤 | 紅燈閃爍 2 次 |
| MODBUS 錯誤 | 紅燈閃爍 2 次 |

---

## 📝 範例資料流

### **RL Mode 範例**

**1. Mesh → M480 (Hex String):**
```
"827601010300000002C40B"
```

**2. 解析:**
- Header: `82 76`
- Type: `01` (GET)
- MODBUS RTU: `01 03 00 00 00 02 C4 0B`

**3. M480 → MODBUS Device (UART0):**
```
01 03 00 00 00 02 C4 0B
```

**4. MODBUS Device → M480:**
```
01 03 02 00 0A 00 0B C4 F0
```

**5. 驗證 CRC → 去除 CRC:**
```
01 03 02 00 0A 00 0B
```

**6. 組裝回應（加上 Header + Type）:**
```
82 76 01 01 03 02 00 0A 00 0B
```

**7. M480 → Mesh (Hex String):**
```
"827601010302000A000B"
```

---

## 🔍 編譯設定

已在 `Smart_Box.cproject.yml` 中加入：
- 編譯時已自動定義 `MODBUS_RTU` 巨集
- `mesh_modbus_agent.c` 加入 User 群組
- 相依的 MODBUS RTU 函式庫已存在

---

## ✨ 特色

1. **零依賴標準庫**：自行實作 `memcpy`、`memset`，避免 `string.h`
2. **模組化設計**：Agent 邏輯完全獨立，易於測試
3. **非阻塞式**：使用輪詢機制，不阻塞主迴圈
4. **共用 MODBUS Client**：與 `modbus_sensor_manager` 共用底層 Client
5. **自動模式偵測**：mesh_handler 自動識別 Agent 訊息

---

## 🧪 測試建議

### **1. 單元測試**
- Hex String ↔ Bytes 轉換
- CRC 計算與驗證
- Header/Type 解析

### **2. 整合測試**
- 使用 MODBUS 模擬器作為 Slave
- 使用 BLE Mesh 測試工具發送訊息
- 測試各種 Function Code (0x03, 0x04, 0x06, 0x10)

### **3. 錯誤測試**
- 故意發送錯誤 CRC 的 MODBUS 回應
- 超時測試（MODBUS Slave 不回應）
- 格式錯誤的 Mesh 訊息

### **4. 壓力測試**
- 連續多次請求
- 邊界條件（最大長度 20 bytes）

---

## 🛠️ 除錯提示

1. **觀察 LED**：
   - 藍燈脈衝：正常回應
   - 紅燈閃爍2次：錯誤

2. **使用 Debugger**：
   - 在 `mesh_modbus_agent_process_mesh_data()` 設中斷點
   - 檢查 `agent->state` 狀態機
   - 觀察 `agent->modbus_response` 內容

3. **UART 監控**：
   - 使用邏輯分析儀監控 UART0 (MODBUS RTU)
   - 驗證發送/接收的原始資料

---

## 📌 注意事項

1. **MODBUS Client 互斥**：
   - `modbus_sensor_manager` 和 `mesh_modbus_agent` 共用同一個 Client
   - 兩者不能同時使用，狀態機會自動處理

2. **記憶體限制**：
   - Mesh 訊息最大 20 bytes
   - MODBUS 回應最大 256 bytes
   - 已針對 M480 RAM 優化

3. **CRC 位元組順序**：
   - MODBUS CRC 為 Little Endian
   - 已在 `calculate_crc16()` 中正確處理

4. **模式切換**：
   - 目前預設 RL Mode
   - 可在 `main.c` 的 `agent_config.mode` 修改

---

## 🎉 完成狀態

✅ **所有功能已實作完成**
✅ **已整合到建置系統**
✅ **準備好進行測試**

---

## 📚 相關檔案清單

```
SampleCode/Smart_Box/
├── mesh_modbus_agent.h          [新增] Agent 標頭檔
├── mesh_modbus_agent.c          [新增] Agent 實作檔
├── mesh_handler.h               [修改] 新增回調定義
├── mesh_handler.c               [修改] 整合 Agent 識別
├── main.c                       [修改] 初始化與輪詢
└── VSCode/
    └── Smart_Box.cproject.yml   [修改] 建置設定
```

---

**實作日期**: 2025年10月16日  
**實作版本**: v1.0  
**作者**: GitHub Copilot
