# MODBUS RTU Agent 通用功能碼支援說明

## 概述
Mesh MODBUS Agent 現已支援**任意 Modbus RTU 功能碼**的完全透傳與回覆，包括標準與自訂功能碼。

## 支援範圍
- **所有標準 Modbus 功能碼**: 0x01 ~ 0x7F (1-127)
- **從站地址**: 0x01 ~ 0xF7 (1-247)
- **未知/自訂功能碼**: 自動使用 Raw 透傳模式

## 實作機制

### 已優化 API 的功能碼
以下功能碼使用專用 API，享有完整的參數驗證與錯誤檢查：

| 功能碼 | 名稱 | API 函式 |
|--------|------|----------|
| 0x01 | Read Coils | `modbus_rtu_client_start_read_coils` |
| 0x02 | Read Discrete Inputs | `modbus_rtu_client_start_read_discrete_inputs` |
| 0x03 | Read Holding Registers | `modbus_rtu_client_start_read_holding` |
| 0x04 | Read Input Registers | `modbus_rtu_client_start_read_input` |
| 0x05 | Write Single Coil | `modbus_rtu_client_start_write_single_coil` |
| 0x06 | Write Single Register | `modbus_rtu_client_start_write_single` |
| 0x0F | Write Multiple Coils | `modbus_rtu_client_start_write_multiple_coils` |
| 0x10 | Write Multiple Registers | `modbus_rtu_client_start_write_multiple` |

### Raw 透傳模式（通用功能碼）
所有**未列於上表的功能碼**會自動使用 `modbus_rtu_client_start_raw()` API：
- ✅ 直接發送完整 RTU 封包（包含 CRC）
- ✅ 接收任意長度的回應
- ✅ 支援標準與非標準功能碼
- ✅ 自動處理 Exception 回應
- ✅ 動態偵測回應長度（基於 byte_count 欄位）

**支援的功能碼範例**：
- 0x07 - Read Exception Status
- 0x0B - Get Comm Event Counter
- 0x0C - Get Comm Event Log
- 0x11 - Report Server ID
- 0x14 - Read File Record
- 0x15 - Write File Record
- 0x16 - Mask Write Register
- 0x17 - Read/Write Multiple Registers
- 0x2B - Read Device Identification
- 以及任何自訂功能碼 (0x41-0x7F)

## 其他功能碼處理
- 未實作專用 API 的功能碼會被 Agent 忽略（`send_modbus_request` 返回 false）
- 若需支援更多功能碼，可：
  1. 在 `modbus_rtu_client.c` 新增對應的 `start_*` API（推薦）
  2. 或在 client 層實作 raw 透傳 API，Agent 未識別功能碼走 raw 路徑

## Mesh 封包格式

### RL Mode (帶 Header)
```
Header(2) + Type(1) + Modbus RTU Package(>=8)
```
- Header: `0x82 0x76`
- Type: 
  - `0x00` = GET (本地 DI 讀取)
  - `0x01` = SET (本地 DO 控制)
  - `0x02` = RTU (Modbus 透傳)
- RTU Package: `slave + func + data... + crc_L + crc_H`

#### 範例：RL Mode 讀取 Coils
```
AT+MDTS 0 827602010000000A5EC5
```
解析：
- `82 76` - Header
- `02` - Type RTU
- `01 01 00 00 00 0A 5E C5` - Modbus RTU: slave=1, func=01, start=0, qty=10, CRC

### Bypass Mode (無 Header)
```
Modbus RTU Package (>=8 bytes)
```
直接發送標準 Modbus RTU 封包。

#### 範例：Bypass Mode 寫入單一線圈
```
AT+MDTS 0 010500010xFF008CFA
```
解析：
- `01` - Slave address
- `05` - Function code (Write Single Coil)
- `00 01` - Coil address = 1
- `FF 00` - Value = ON (0xFF00=ON, 0x0000=OFF)
- `8C FA` - CRC16

## 回覆格式

### RL Mode 回覆
```
Header(2) + Type(1) + Modbus Response(去除CRC)
```
成功範例（Read Coils 回應）：
```
82 76 02 01 01 02 CD
```
解析：
- `82 76` - Header
- `02` - Type RTU
- `01 01 02 CD` - Modbus response (slave + func + byte_count + data...)

錯誤範例（CRC 錯誤或 Timeout）：
```
82 76 02 FE
```
- `FE` = CRC_FAILED / TIMEOUT
- `FD` = BUSY

### Bypass Mode 回覆
```
Modbus Response (去除CRC)
```
直接回傳 Modbus 回應本體。

## 使用流程

1. **準備 Modbus RTU 封包**
   - 計算 CRC16 (Modbus standard)
   - 確保封包格式正確

2. **選擇模式**
   - RL Mode: 加上 `82 76 02` header
   - Bypass Mode: 直接使用 RTU 封包

3. **轉換為 Hex String**
   ```
   範例: 01 05 00 01 FF 00 8C FA
   轉換: "010500010xFF008CFA"
   ```

4. **透過 Mesh 發送**
   ```
   AT+MDTS 0 <hex_string>
   ```

5. **接收回覆**
   - 監聽 `+MDTR:` 事件
   - Agent 會自動回傳處理結果

## 測試範例

### 讀取線圈 (0x01)
```bash
# RL Mode
AT+MDTS 0 827602010100000001DDCA

# Bypass Mode  
AT+MDTS 0 010100000001DDCA
```

### 讀取離散輸入 (0x02)
```bash
# Bypass Mode
AT+MDTS 0 01020005000A9D8E
```

### 寫入單一線圈 (0x05)
```bash
# Bypass Mode - 開啟線圈 0
AT+MDTS 0 01050000FF008C3A
```

### 寫入多個線圈 (0x0F)
```bash
# Bypass Mode - 寫入 8 個線圈 (byte=0xCD)
AT+MDTS 0 010F000000080101CD????
# (CRC 需自行計算)
```

### 自訂/未知功能碼 (Raw 模式)
```bash
# 範例：功能碼 0x2B (Read Device Identification)
# MEI Type = 0x0E, Read Device ID Code = 0x01, Object ID = 0x00
AT+MDTS 0 012B0E010000????
# (CRC 需自行計算)

# 範例：自訂功能碼 0x50
AT+MDTS 0 01501234ABCD????
# (CRC 需自行計算)
```

## 限制與注意事項

1. **單一請求處理**
   - Agent 同一時間只能處理一個 RTU 請求
   - 若 Agent 忙碌，新請求會被緩衝或丟棄

2. **CRC 驗證**
   - 發送時必須包含正確的 CRC16
   - 回覆會驗證 CRC，錯誤時返回 ERROR_CODE_CRC_FAILED

3. **Timeout**
   - 預設 Modbus timeout: 300ms
   - 預設最大等待: 500ms
   - 可在 `main.c` 初始化時調整

4. **封包長度**
   - Bypass Mode: >= 8 bytes
   - RL Mode: >= 11 bytes (header + type + RTU)
   - 最大長度受限於 buffer 大小 (256 bytes)

5. **Raw 模式回應偵測**
   - 自動偵測回應長度（基於第 3 byte 的 byte_count）
   - 適用於大多數讀取型功能碼
   - 寫入型功能碼通常固定 8 bytes 回應
   - Exception 回應固定 5 bytes (slave + func|0x80 + exception + CRC)

6. **通用性保證**
   - ✅ 所有標準 Modbus 功能碼均可使用
   - ✅ 支援廠商自訂功能碼 (0x41-0x7F)
   - ✅ 自動適應各種回應格式
   - ⚠️ 非標準格式可能需要手動處理回應長度

## 架構說明

```
Mesh MDTR → mesh_handler → agent_mesh_data_callback
                              ↓
                    mesh_modbus_agent_process_mesh_data
                              ↓
                      send_modbus_request (功能碼分派)
                         ↙          ↘
            已知功能碼            未知功能碼
            (0x01-0x10)          (Raw 透傳)
                 ↓                    ↓
    modbus_rtu_client_      modbus_rtu_client_
       start_xxx()              start_raw()
                 ↘                  ↙
                      UART RS485 → Modbus Device
                              ↓
                    modbus_rtu_client (回應解析)
                         ↙          ↘
                  標準回應         動態長度偵測
                      ↘          ↙
                    agent_response_ready_callback
                              ↓
                    AT+MDTS (回傳給 Mesh)
```

## Raw 透傳實作細節

### 請求處理
```c
// Agent 未識別的功能碼會走 raw 路徑
else
{
    send_ok = modbus_rtu_client_start_raw(
        agent->modbus_client,
        agent->modbus_request,      // 完整 RTU 封包(含 CRC)
        agent->modbus_request_length,
        agent->config.modbus_timeout_ms);
}
```

### 回應長度偵測
Raw 模式下，client 會自動嘗試偵測回應長度：

1. **讀取前 3 bytes**: slave + func + byte_count
2. **計算期望長度**: 3 + byte_count + 2 (CRC)
3. **等待完整封包**: 直到達到期望長度或 timeout
4. **驗證 CRC**: 確保資料完整性

**適用格式**（大多數讀取型功能碼）:
```
[slave][func][byte_count][data...][CRC_L][CRC_H]
```

**固定長度回應**（寫入型功能碼通常為 8 bytes）:
```
[slave][func][addr_H][addr_L][value_H][value_L][CRC_L][CRC_H]
```

**Exception 回應**（5 bytes）:
```
[slave][func|0x80][exception_code][CRC_L][CRC_H]
```

## 擴充指南

### 新增功能碼支援

1. **在 `modbus_rtu_client.h` 新增常數**
   ```c
   #define MODBUS_RTU_FUNCTION_XXX (0xXXU)
   ```

2. **在 `modbus_rtu_client.c` 實作 API**
   ```c
   bool modbus_rtu_client_start_xxx(...)
   {
       // 構建請求封包
       // 發送
       // 設定期望回應
   }
   ```

3. **更新回應解析**
   - 在 `modbus_rtu_client_handle_rx_byte` 加入該功能碼的 expected_length 計算

4. **在 `mesh_modbus_agent.c` 新增分支**
   ```c
   else if (func_code == MODBUS_RTU_FUNCTION_XXX)
   {
       // 解析參數
       // 呼叫 modbus_rtu_client_start_xxx
   }
   ```

## 相關文件
- `MESH_MODBUS_AGENT_USAGE.md` - Agent 使用指南
- `modbus_rtu/README.md` - RTU Client 文件
- `DOCS_MODBUS_BLE.md` - 整體架構說明
