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

有關回應組裝、raw 模式細節與錯誤處理的完整實作說明請參見 `docs/IMPLEMENTATION_NOTES.md`，以及 `SampleCode/Smart_Box/modbus_rtu/` 目錄下的範例程式。
