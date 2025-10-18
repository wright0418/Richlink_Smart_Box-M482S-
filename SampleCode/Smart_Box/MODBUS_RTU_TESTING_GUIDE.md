# MODBUS RTU 通用功能碼測試指南

## 快速驗證

### 1. 測試標準功能碼（已優化）

#### Read Coils (0x01)
```bash
# 請求: slave=1, func=01, start=0, qty=16
# 計算 CRC: 01 01 00 00 00 10 → CRC = 3D CA
AT+MDTS 0 0101000000103DCA

# 預期回應範例 (Bypass Mode):
# 01 01 02 FF 00 CRC...
# 解析: slave=1, func=01, byte_count=2, data=FF 00 (16 coils)
```

#### Write Single Coil (0x05)
```bash
# 請求: slave=1, func=05, addr=10, value=ON(0xFF00)
# 計算 CRC: 01 05 00 0A FF 00 → CRC = AC 3D
AT+MDTS 0 0105000AFF00AC3D

# 預期回應:
# 01 05 00 0A FF 00 AC 3D (echo request)
```

### 2. 測試進階功能碼（Raw 透傳）

#### Read Exception Status (0x07)
```bash
# 請求: slave=1, func=07
# 計算 CRC: 01 07 → CRC = 01 0C
AT+MDTS 0 0107010C

# 預期回應範例:
# 01 07 01 6D CRC...
# 解析: slave=1, func=07, status=0x6D
```

#### Read Device Identification (0x2B)
```bash
# 請求: slave=1, func=2B, MEI=0x0E, ReadDevId=01, ObjId=00
# 計算 CRC: 01 2B 0E 01 00 → CRC = 需計算
AT+MDTS 0 012B0E0100????

# 預期回應範例:
# 01 2B 0E 01 01 00 00 03 00 ... CRC
# (回應格式依設備而異)
```

#### Mask Write Register (0x16)
```bash
# 請求: slave=1, func=16, addr=4, AND_Mask=00F2, OR_Mask=0025
# 計算 CRC: 01 16 00 04 00 F2 00 25 → CRC = 需計算
AT+MDTS 0 011600040F020025????

# 預期回應:
# Echo request (8 bytes total)
```

### 3. 測試自訂功能碼

#### 廠商自訂 0x50
```bash
# 請求: slave=1, func=50, custom_data=1234ABCD
# 計算 CRC: 01 50 12 34 AB CD → CRC = 需計算
AT+MDTS 0 01501234ABCD????

# 回應格式依設備定義
```

## CRC 計算工具

### Python 腳本
```python
#!/usr/bin/env python3
def modbus_crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

# 使用範例
request = bytes.fromhex("0101000000010")
crc = modbus_crc16(request)
print(f"CRC: {crc:04X} = {crc & 0xFF:02X} {(crc >> 8) & 0xFF:02X}")

# 完整封包
full_frame = request + bytes([crc & 0xFF, (crc >> 8) & 0xFF])
print(f"Full: {full_frame.hex().upper()}")
```

### 線上工具
- https://www.lammertbies.nl/comm/info/crc-calculation
- 選擇 "CRC-16 (Modbus)"

## 測試流程

### RL Mode 測試
```bash
# 1. 準備 RTU 封包
RTU_PACKET="0101000000103DCA"

# 2. 加上 RL header
RL_PACKET="827602${RTU_PACKET}"

# 3. 發送
AT+MDTS 0 ${RL_PACKET}

# 4. 等待回應 +MDTR:
# 預期: 827602 + MODBUS_RESPONSE (without CRC)
```

### Bypass Mode 測試
```bash
# 直接發送 RTU 封包
AT+MDTS 0 0101000000103DCA

# 預期回應: MODBUS_RESPONSE (without CRC)
```

## 常見回應格式

### 成功讀取（含 byte_count）
```
[slave][func][byte_count][data N bytes][CRC_L][CRC_H]
```
範例：
- Read Coils: `01 01 02 CD AB CRC...`
- Read Holding: `01 03 04 12 34 56 78 CRC...`

### 成功寫入（Echo 式）
```
[slave][func][addr_H][addr_L][value/qty_H][value/qty_L][CRC_L][CRC_H]
```
範例：
- Write Single: `01 06 00 01 00 03 CRC...`
- Write Multiple: `01 10 00 00 00 02 CRC...`

### Exception 回應
```
[slave][func|0x80][exception_code][CRC_L][CRC_H]
```
範例：
- Illegal Function: `01 81 01 CRC...` (0x01)
- Illegal Address: `01 83 02 CRC...` (0x02)
- Illegal Value: `01 86 03 CRC...` (0x03)

### 特殊格式（依功能碼）
```
Read Exception Status (0x07):
[slave][07][status_byte][CRC_L][CRC_H]

Get Comm Event Counter (0x0B):
[slave][0B][status_H][status_L][event_H][event_L][CRC_L][CRC_H]

Read Device ID (0x2B):
[slave][2B][MEI][ReadDevId][conformity][more][next][num_objs][obj_data...][CRC_L][CRC_H]
```

## 除錯技巧

### 1. 啟用詳細日誌
查看 LED 指示：
- 黃燈閃 1 次：收到新 Mesh request
- 黃燈閃 3 次：成功發送 MODBUS request
- 黃燈閃 2 次 + 紅燈：MODBUS 錯誤/timeout
- 紅燈持續：Agent 初始化失敗

### 2. 檢查 UART 波形
使用邏輯分析儀監控 RS485：
- 確認 TX 封包正確
- 檢查 RX 回應格式
- 驗證 CRC 正確性

### 3. 模擬從站回應
使用 Modbus Slave Simulator：
- 配置虛擬從站 (address=1)
- 啟用所有功能碼
- 觀察實際回應格式

### 4. 常見問題排查

#### 問題：收到 BUSY 錯誤 (0xFD)
- **原因**：Sensor Manager 正在輪詢
- **解決**：等待 200ms 後重試

#### 問題：收到 CRC_FAILED 錯誤 (0xFE)
- **原因**：CRC 計算錯誤或傳輸損壞
- **解決**：使用工具重新計算 CRC

#### 問題：Timeout 無回應
- **原因**：從站未回應或地址錯誤
- **解決**：
  1. 檢查從站地址
  2. 確認波特率匹配
  3. 檢查 RS485 硬體連線

#### 問題：Raw 模式回應長度錯誤
- **原因**：回應格式不含 byte_count 欄位
- **解決**：
  1. 查閱設備手冊確認回應格式
  2. 可能需要修改 client 的長度偵測邏輯
  3. 或在已知功能碼列表中新增專用 API

## 進階測試案例

### 連續多功能碼測試
```bash
# 1. Read Holding (0x03)
AT+MDTS 0 010300000002C40B

# 2. Write Single (0x06)
AT+MDTS 0 010600010003DA0A

# 3. Read Input (0x04)
AT+MDTS 0 010400000002710B

# 4. Read Coils (0x01)
AT+MDTS 0 0101000000083DCD

# 5. Read Device ID (0x2B) - Raw
AT+MDTS 0 012B0E010000????
```

### 壓力測試
```bash
# 快速連續發送（測試緩衝與互斥）
for i in {1..10}; do
  AT+MDTS 0 0103000000010C0D
  sleep 0.5
done
```

### 混合模式測試
```bash
# RL Mode
AT+MDTS 0 8276020103000000010C0D

# Bypass Mode
AT+MDTS 0 0103000000010C0D

# RL GET (local)
AT+MDTS 0 82760000

# RL SET (local)
AT+MDTS 0 82760100001
```

## 預期結果摘要

| 功能碼 | 類型 | 處理方式 | 回應格式 |
|--------|------|----------|----------|
| 0x01-0x10 (已知) | 專用 API | 參數驗證 + 長度檢查 | 標準格式 |
| 0x07, 0x0B, 0x2B... | Raw 透傳 | 直接發送 + 動態長度 | 依設備 |
| 0x41-0x7F (自訂) | Raw 透傳 | 完整透傳 | 依廠商定義 |
| Exception | 自動處理 | CRC 驗證 | 5 bytes 固定 |

## 參考資料
- Modbus Application Protocol V1.1b3
- `MODBUS_RTU_AGENT_EXTENDED.md` - 完整功能說明
- `modbus_rtu/README.md` - Client API 文件
