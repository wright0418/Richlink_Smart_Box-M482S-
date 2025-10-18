# Mesh Modbus Agent - Type 測試指南

## 概述
本文件說明 mesh_modbus_agent 支援的三種 TYPE，以及各自的封包格式與處理流程。

## 支援的 TYPE

### 1. RL_TYPE_GET (0x00) - 本地 DI 讀取
**功能**：讀取數位輸入（PB.7）狀態

**請求格式**：
```
Header(2) + Type(1) + IO_ADDR(1)
範例：82 76 00 00
```

**回應格式**：
```
Header(2) + Type(1) + STATUS_OK(0x80) + IO_ADDR(1) + DI(1)
範例（DI=1）：82 76 00 80 00 01
範例（DI=0）：82 76 00 80 00 00
```

**處理流程**：
- ✅ 本地處理，不經過 Modbus RTU
- ✅ 立即回應，無等待時間
- ✅ 讀取 PB.7 腳位狀態（active-low）
- ✅ 回傳 0x01（high）或 0x00（low）

---

### 2. RL_TYPE_SET (0x01) - 本地 DO 控制
**功能**：控制數位輸出（PA.6 繼電器）

**請求格式**：
```
Header(2) + Type(1) + IO_ADDR(1) + VALUE(1)
範例（開）：82 76 01 00 01
範例（關）：82 76 01 00 00
```

**回應格式**：
```
Header(2) + Type(1) + STATUS_OK(0x80) + IO_ADDR(1) + DO(1)
範例（DO=1）：82 76 01 80 00 01
範例（DO=0）：82 76 01 80 00 00
```

**處理流程**：
- ✅ 本地處理，不經過 Modbus RTU
- ✅ 立即回應，無等待時間
- ✅ 根據 VALUE 控制 PA.6 輸出
- ✅ 回讀並回傳當前 PA.6 狀態

---

### 3. RL_TYPE_RTU (0x02) - Modbus RTU 透傳【完整保留】
**功能**：透過 Modbus RTU 與下游設備通訊

**請求格式**：
```
Header(2) + Type(1) + Modbus_ADU(≥8)
範例（Read Holding Reg）：82 76 02 01 03 00 00 00 01 84 0A
```

**回應格式**：
```
Header(2) + Type(1) + Modbus_Response(無CRC)
範例（成功）：82 76 02 01 03 02 00 64
範例（忙碌）：82 76 02 FD
範例（錯誤）：82 76 02 FE
```

**處理流程**：
- ✅ **完整保留原有 Modbus RTU 流程**
- ✅ 支援 Function Code：
  - 0x03: Read Holding Registers
  - 0x04: Read Input Registers
  - 0x06: Write Single Register
  - 0x10: Write Multiple Registers
- ✅ 非同步處理（進入 WAITING_MODBUS_RESPONSE 狀態）
- ✅ CRC 驗證後去除 CRC 回傳
- ✅ LED 指示（黃燈 1→3 閃）
- ✅ 錯誤處理（超時、CRC 錯誤、忙碌）

---

## 程式碼流程驗證

### RTU 流程完整性檢查

#### 1. 解析階段（parse_rl_mode_data）
```c
if (agent->rl_type == RL_TYPE_RTU)  // 0x02
{
    // 檢查長度 >= 11（header + type + 最小 Modbus ADU）
    if (length < 11) return false;
    
    // 複製 Modbus ADU 到 modbus_request
    agent->modbus_request_length = length - 3;
    my_memcpy(agent->modbus_request, &data[3], agent->modbus_request_length);
}
```
✅ **與原始程式碼完全相同**

#### 2. 分派階段（mesh_modbus_agent_process_mesh_data）
```c
// GET/SET 提前返回，RTU 繼續往下
if (agent->config.mode == MESH_MODBUS_AGENT_MODE_RL)
{
    if (agent->rl_type == RL_TYPE_GET) {  // 0x00
        rl_handle_get(agent, data, length);
        return true;  // 提前返回
    }
    else if (agent->rl_type == RL_TYPE_SET) {  // 0x01
        rl_handle_set(agent, data, length);
        return true;  // 提前返回
    }
    // RTU (type == 0x02) 不進入上面條件，繼續執行下方
}

// RTU 執行到這裡
if (!send_modbus_request(agent)) { ... }
```
✅ **RTU 路徑完整保留**

#### 3. 發送階段（send_modbus_request）
```c
// 驗證 modbus_request_length（RTU 必定 >= 8）
if (agent->modbus_request_length < 4) return false;

// 解析並呼叫對應 API
uint8_t func_code = agent->modbus_request[1];
if (func_code == MODBUS_RTU_FUNCTION_READ_HOLDING) {
    modbus_rtu_client_start_read_holding(...);
}
// ... 其他 function codes
```
✅ **與原始程式碼完全相同**

#### 4. 回應階段（prepare_mesh_response）
```c
// RL Mode 回應格式
agent->mesh_tx_buffer[0] = agent->rl_header[0];  // 0x82
agent->mesh_tx_buffer[1] = agent->rl_header[1];  // 0x76
agent->mesh_tx_buffer[2] = agent->rl_type;        // 0x02 (RTU)
// 接著是 Modbus response（已去除 CRC）
```
✅ **與原始程式碼完全相同**

---

## 測試案例

### 測試案例 1：GET（本地處理）
```
請求：82 76 00 00
預期回應：82 76 00 80 00 [DI狀態]
處理時間：< 1ms（立即回應）
```

### 測試案例 2：SET（本地處理）
```
請求：82 76 01 00 01（開繼電器）
預期回應：82 76 01 80 00 01
處理時間：< 1ms（立即回應）
```

### 測試案例 3：RTU Read Holding（Modbus 透傳）
```
請求：82 76 02 01 03 00 00 00 01 84 0A
預期回應：82 76 02 01 03 02 [資料...]
處理時間：取決於 Modbus 設備回應（有 timeout）
LED 指示：黃燈閃爍（1→3）
```

### 測試案例 4：RTU Write Single（Modbus 透傳）
```
請求：82 76 02 01 06 00 00 00 64 88 1E
預期回應：82 76 02 01 06 00 00 00 64
處理時間：取決於 Modbus 設備回應（有 timeout）
```

---

## 結論

✅ **所有三種 TYPE 功能完整**
✅ **RTU 流程與原始程式碼完全相同，未被修改破壞**
✅ **GET/SET 為新增功能，不影響既有 RTU 路徑**
✅ **已通過靜態程式碼分析與邏輯追蹤驗證**

## 故障排除

如果 RTU 實際測試時出現問題，請檢查：

1. **Modbus RTU Client 初始化**：確認 modbus_client 已正確初始化
2. **硬體連線**：確認 RS485 連線正常（PB14 DIR 控制）
3. **Slave 位址**：確認下游設備位址正確
4. **Timeout 設定**：確認 config.modbus_timeout_ms 設定合理
5. **CRC 計算**：確認請求封包的 CRC 正確

若仍有問題，請提供：
- 實際發送的封包（hex）
- 預期回應
- 實際回應（或無回應）
- LED 閃爍模式
- 任何錯誤訊息
