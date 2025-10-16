# 規格需求
在目前的架構下增加一個 MESH_MODBUS_Agent_mode

# 目的
使用 MESH DEVICE Module 接收 MODBUS RTU REQUEST資料，並轉發到使用 UART MODBUS CLIENT 發給裝置，裝置回傳的內容去除 CRC 後再轉發回 MESH DEVICE module

# 規則
- 持續監聽 MESH DEVICE Module 傳來的資料
  ## RL Ddevice Mode
  - 接收到的資料是 Hex string 最大 20byes
  - 資料格式  header(2byte) + type (1byte) + MODBUS RTU package (8bytes)
    - header = 0x82 0x76
    - type = 0x01 (GET) / 0x02 (SET) / 0x03 (RTU)
    - MODBUS RTU package = MODBUS RTU ID (1byte) + function code (1byte) + start address (2byte) + quantity of registers (2byte) + CRC (2byte)
    - 範例: 827601010300000002C40B
  - 將 MODBUS RTU package 透過 UART MODBUS CLIENT 發送給裝置
  - 等待裝置回傳資料
  - 裝置回傳資料格式: header (2byte) +type (1byte) + MODBUS RTU ID (1byte) + function code (1byte) + byte count (1byte) + data (N byte) + CRC (2byte)
    - 範例: 82760103010302000A000BC4F0
    - N = quantity of registers * 2
    - 需要檢查 CRC 是否正確
    - 若 CRC 不正確 (閃紅燈兩次)，則回傳錯誤訊息給 MESH DEVICE module : header(2byte) + type(1byte) + error code(1byte) 0xFE 
      - 範例: 827601FE
    - 若 CRC 正確，則去除 CRC 後進行回傳
    - 範例: 010302000A000BC4
    - 將去除 CRC 後的資料透過 MESH DEVICE Module (需使用 Hex string 格式) 回傳給 MESH DEVICE module  

# Bypass Mode
  ## RL Ddevice Mode
  - 接收到的資料是 Hex string 最大 20byes
  - 資料格式  MODBUS RTU package (8bytes)
    - MODBUS RTU package = MODBUS RTU ID (1byte) + function code (1byte) + start address (2byte) + quantity of registers (2byte) + CRC (2byte)
    - 範例: 010300000002C40B
  - 將 MODBUS RTU package 透過 UART MODBUS CLIENT 發送給裝置
  - 等待裝置回傳資料
  - 裝置回傳資料格式: MODBUS RTU ID (1byte) + function code (1byte) + byte count (1byte) + data (N byte) + CRC (2byte)
    - 範例: 010302000A000BC4F0
    - N = quantity of registers * 2
    - 需要檢查 CRC 是否正確
    - 若 CRC 不正確，不回傳 (閃紅燈2次)
    - 若 CRC 正確，則去除 CRC 後進行回傳
    - 範例: 010302000A000BC4
    - 將去除 CRC 後的資料透過 MESH DEVICE Module (需使用 Hex string 格式) 回傳給 MESH DEVICE module  
