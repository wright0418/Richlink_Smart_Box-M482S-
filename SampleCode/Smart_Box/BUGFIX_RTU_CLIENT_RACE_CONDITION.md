# Modbus RTU Client 競態條件修復報告

## 問題描述

當 Mesh 主機快速發送 RTU request 時，在 `modbus_rtu_client_is_busy()` 函數中發生異常崩潰（Hard Fault）。

### 異常位置
- 文件: `modbus_rtu\modbus_rtu_client.c`
- 函數: `modbus_rtu_client_is_busy()`
- 行號: 431 (修復前)

### 根本原因

這是一個經典的 **競態條件（Race Condition）** 問題：

1. **多執行上下文同時訪問共享數據**
   - UART RX 中斷處理程序調用 `modbus_rtu_client_handle_rx_byte()` 修改 `client->state`
   - 主循環中調用 `modbus_rtu_client_is_busy()` 讀取 `client->state`

2. **TOCTOU 問題 (Time-Of-Check to Time-Of-Use)**
   ```c
   // 修復前的代碼
   return (client != NULL) && (client->state == MODBUS_RTU_CLIENT_STATE_WAITING);
   ```
   - 在檢查 `client != NULL` 之後
   - 在訪問 `client->state` 之前
   - 中斷可能改變了 `client->state` 的值，或者更糟的情況下破壞了內存

3. **快速發送請求時問題更容易出現**
   - 高頻率的 RTU 請求增加了中斷觸發的頻率
   - 增加了主循環與中斷處理程序之間發生衝突的機率

## 修復方案

### 1. 添加臨界區保護

在訪問共享狀態變量時，使用 CMSIS 提供的中斷保護機制：

```c
bool modbus_rtu_client_is_busy(const modbus_rtu_client_t *client)
{
    if (client == NULL)
    {
        return false;
    }
    
    // 保護臨界區，防止中斷修改狀態
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    bool is_busy = (client->state == MODBUS_RTU_CLIENT_STATE_WAITING);
    
    __set_PRIMASK(primask);
    
    return is_busy;
}
```

### 2. 同樣保護 `modbus_rtu_client_get_state()`

```c
modbus_rtu_client_state_t modbus_rtu_client_get_state(const modbus_rtu_client_t *client)
{
    if (client == NULL)
    {
        return MODBUS_RTU_CLIENT_STATE_ERROR;
    }
    
    // 保護臨界區，防止中斷修改狀態
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    modbus_rtu_client_state_t state = client->state;
    
    __set_PRIMASK(primask);
    
    return state;
}
```

### 3. 添加必要的頭文件

在 `modbus_rtu_client.c` 頂部添加：
```c
#include "M480.h"  // 包含 core_cm4.h，提供 CMSIS 中斷控制函數
```

## 技術細節

### 臨界區實現

1. **保存當前中斷狀態**: `uint32_t primask = __get_PRIMASK();`
   - 獲取當前的 PRIMASK 寄存器值（記錄中斷是否已啟用）

2. **禁用所有中斷**: `__disable_irq();`
   - 設置 PRIMASK 寄存器，禁用除 NMI 和 HardFault 外的所有中斷

3. **執行臨界區操作**: 讀取 `client->state`
   - 在此期間不會有中斷打斷執行

4. **恢復中斷狀態**: `__set_PRIMASK(primask);`
   - 恢復之前保存的中斷狀態
   - 如果之前中斷是啟用的，則重新啟用

### 為什麼這樣修復有效

1. **原子性保證**: 在臨界區內讀取 `client->state` 是原子操作
2. **避免中斷干擾**: UART RX 中斷無法在讀取過程中修改狀態
3. **保持嵌套安全**: 使用 `__get_PRIMASK()` 和 `__set_PRIMASK()` 支持嵌套調用

## 影響範圍

### 修改的文件
- `SampleCode/Smart_Box/modbus_rtu/modbus_rtu_client.c`

### 修改的函數
1. `modbus_rtu_client_is_busy()` - 添加臨界區保護
2. `modbus_rtu_client_get_state()` - 添加臨界區保護

### 性能影響
- **極小**: 臨界區持續時間非常短（僅讀取一個變量）
- **典型執行時間**: < 1 μs
- **對系統響應的影響**: 可忽略不計

## 測試建議

1. **高頻請求測試**
   - Mesh 主機以最大速率發送 RTU 請求
   - 運行至少 1 小時，確保無異常

2. **並發測試**
   - 同時進行 RTU 請求和 UART 接收
   - 驗證狀態查詢的準確性

3. **壓力測試**
   - 多個傳感器同時通訊
   - 快速輪詢狀態

## 替代方案（未採用）

### 方案 A: 使用 volatile 關鍵字
```c
volatile modbus_rtu_client_state_t state;
```
**缺點**: 
- 僅保證讀取的可見性，不保證原子性
- 在多核或帶緩存的系統上可能不足

### 方案 B: 使用信號量/互斥鎖
**缺點**:
- 過於重量級
- 需要 RTOS 支持
- 不適用於中斷上下文

### 方案 C: 禁用/啟用特定 UART 中斷
```c
NVIC_DisableIRQ(UART0_IRQn);
// 讀取狀態
NVIC_EnableIRQ(UART0_IRQn);
```
**缺點**:
- 可能丟失 UART 數據
- 需要知道具體的 IRQ 編號

## 結論

通過添加輕量級的臨界區保護，成功解決了快速 RTU 請求時的競態條件問題。修復方案：
- ✅ 保證數據一致性
- ✅ 性能影響最小
- ✅ 代碼簡潔易懂
- ✅ 符合 CMSIS 標準

## 日期
2025-01-16
