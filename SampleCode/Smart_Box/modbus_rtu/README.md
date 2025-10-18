# Modbus RTU Server 使用指南

## 概述

本 Modbus RTU Server 是專為 Nuvoton M480 系列微控制器設計的完整 Modbus RTU 實現，支援硬體/軟體雙重 CRC 計算、精確時序控制和完整的協議處理。

## 硬體配置

### UART 腳位
- **UART0_RXD**: PD.2 (接收)
- **UART0_TXD**: PD.3 (發送)
- **RS485 DIR**: PB.14 (方向控制)

### 預設設定
- **波特率**: 9600 bps
- **資料位元**: 8-bit
- **停止位元**: 1-bit  
- **校驗位元**: None

## 自我測試功能

`modbus_rtu_run_module_self_test()` 函數會執行以下測試：

### 1. CRC 模組測試
測試向量：`{0x01, 0x04, 0x02, 0xFF, 0xFF}`  
期望 CRC16：`0x80B8`

- 自動檢測 M480 硬體 CRC 模組相容性
- 測試 16 種不同的 CRC 屬性組合
- 若硬體 CRC 不相容，自動回退到軟體計算

### 2. Modbus RTU 協議測試
- **寫入單一暫存器 (0x06)**: 寫入值 `0x1234` 到地址 `0x0002`
- **讀取保持暫存器 (0x03)**: 讀取地址 `0x0002-0x0003` 共 2 個暫存器
- **廣播寫入多重暫存器 (0x10)**: 廣播寫入到地址 `0x0004-0x0005`

## 基本用法

### 2. 系統初始化
```c
void SYS_Init(void)
{
    // ... 其他初始化 ...
    
#ifdef MODBUS_RTU
    // 啟用 UART0 模組時鐘 (MODBUS RTU 使用)
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HXT, CLK_CLKDIV0_UART0(1));
    
    // 配置 UART0 腳位 (PD2=RXD, PD3=TXD)
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD2MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD2MFP_UART0_RXD | SYS_GPD_MFPL_PD3MFP_UART0_TXD);
    
    // 配置 RS485 DIR 腳位
    SYS->GPB_MFPH &= ~SYS_GPB_MFPH_PB14MFP_Msk;
    SYS->GPB_MFPH |= SYS_GPB_MFPH_PB14MFP_GPIO;
#endif
}
```

### 3. GPIO 初始化
```c
void digital_io_init(void)
{
    // ... 其他 GPIO 初始化 ...
    
#ifdef MODBUS_RTU
    // PB.14 作為 RS485 DIR 控制腳
    GPIO_SetMode(PB, BIT14, GPIO_MODE_OUTPUT);
    PB14 = 0; // 預設接收模式
#endif
}
```

### 4. 主程式中執行自我測試
```c
int main()
{
    SYS_Init();
    digital_io_init();
    
    // 設定 SysTick
    SysTick_Config(SystemCoreClock / 1000);
    
#ifdef MODBUS_RTU
    // 開啟 UART0
    UART_Open(UART0, 9600);
    
    // 執行自我測試
    g_modbus_rtu_self_test_pass = modbus_rtu_run_module_self_test();
    
    // 測試失敗時點亮紅燈
    if (!g_modbus_rtu_self_test_pass)
    {
        red_led_on();
        g_red_on_until_ms = 0; // 持續亮起
    }
#endif
    
    while (1)
    {
        // 主迴圈持續檢查測試狀態
#ifdef MODBUS_RTU
        if (!g_modbus_rtu_self_test_pass)
        {
            red_led_on(); // 保持紅燈亮起表示測試失敗
        }
#endif
        
        // ... 其他主迴圈處理 ...
    }
}
```

## 進階用法

### 實際 Modbus RTU Server 實現

```c
#include "modbus_rtu_server.h"
#include "uart_rs485_driver.h"

// 定義暫存器資料結構
typedef struct {
    uint16_t holding_registers[100];
    uint16_t input_registers[100];
} modbus_registers_t;

static modbus_registers_t g_modbus_regs;
static modbus_rtu_server_t g_modbus_server;

// 實現回調函數
static bool read_holding_registers(void *context, uint16_t address, uint16_t quantity, uint16_t *buffer)
{
    modbus_registers_t *regs = (modbus_registers_t *)context;
    
    if (address + quantity > 100) {
        return false; // 地址超出範圍
    }
    
    for (uint16_t i = 0; i < quantity; i++) {
        buffer[i] = regs->holding_registers[address + i];
    }
    
    return true;
}

static bool write_single_register(void *context, uint16_t address, uint16_t value)
{
    modbus_registers_t *regs = (modbus_registers_t *)context;
    
    if (address >= 100) {
        return false; // 地址超出範圍
    }
    
    regs->holding_registers[address] = value;
    return true;
}

// 初始化 Modbus RTU Server
void modbus_server_init(void)
{
    // 清空暫存器
    memset(&g_modbus_regs, 0, sizeof(g_modbus_regs));
    
    // 設定回調函數
    modbus_rtu_callbacks_t callbacks = {
        .read_holding = read_holding_registers,
        .read_input = NULL, // 可選
        .write_single = write_single_register,
        .write_multiple = NULL // 可選
    };
    
    // 配置 Server
    modbus_rtu_server_config_t config = {
        .slave_address = 1,
        .baudrate = 9600,
        .callbacks = &callbacks,
        .callback_context = &g_modbus_regs,
        .tx_handler = uart_tx_handler, // 需要實現
        .tx_context = NULL,
        .timestamp_callback = get_system_time_ms,
        .timestamp_context = NULL,
        .crc_method = MODBUS_CRC_METHOD_AUTO
    };
    
    // 初始化 Server
    modbus_rtu_server_init(&g_modbus_server, &config);
}
```

## 編譯選項

### 啟用 Modbus RTU
在 `Smart_Box.cproject.yml` 中已自動定義：
```yaml
C: [-DMODBUS_RTU]
```

### 相關源檔案
專案已自動包含以下檔案：
- `modbus_crc.c` - CRC 計算模組
- `modbus_timing.c` - 時序計算
- `modbus_rtu_protocol.c` - 協議處理
- `modbus_rtu_server.c` - Server 實現
- `uart_rs485_driver.c` - UART/RS485 驅動
- `modbus_rtu_test.c` - 自我測試
- `../../../Library/StdDriver/src/crc.c` - M480 CRC 驅動

## 狀態指示

### LED 指示燈
- **紅燈亮起**: Modbus RTU 自我測試失敗
- **紅燈熄滅**: 自我測試通過

### 測試項目檢查
```c
// 檢查測試狀態
if (g_modbus_rtu_self_test_pass) {
    printf("Modbus RTU 自我測試通過\\n");
} else {
    printf("Modbus RTU 自我測試失敗\\n");
}
```

## 支援的功能碼

目前實現的 Modbus 功能碼：
- **0x03**: 讀取保持暫存器
- **0x04**: 讀取輸入暫存器
- **0x06**: 寫入單一暫存器
- **0x10**: 寫入多個暫存器

## 時序規範

符合 Modbus RTU 標準：
- **字元間隔**: ≤ 1.5 字元時間
- **訊框間隔**: ≥ 3.5 字元時間
- **高速率優化**: > 19200 bps 時使用固定時間值

## 錯誤處理

系統提供完整的錯誤處理：
- CRC 錯誤檢測
- 訊框溢位保護
- 非法功能碼回應
- 地址範圍檢查

## 調試與測試

### 使用 Modbus 測試工具
推薦使用以下工具測試：
- **QModMaster** (免費)
- **ModbusPoll** (商業)
- **Python pymodbus** (程式化測試)

### 測試設定
- **連接埠**: 根據實際 COM 埠
- **波特率**: 9600
- **資料位元**: 8, 停止位元: 1, 校驗: None
- **從機地址**: 1 (預設)

## 注意事項

1. **時鐘配置**: 確保系統時鐘正確設定為 192MHz
2. **UART 腳位**: 確認 PD.0/PD.1 未被其他功能佔用
3. **RS485 方向**: PB.14 必須正確控制 RS485 收發方向
4. **記憶體使用**: 每個 Server 實例約使用 300+ bytes RAM
5. **中斷優先權**: UART 中斷應設定適當優先權

## 擴展功能

可根據需要擴展：
- 更多 Modbus 功能碼
- 多個從機地址支援
- 寄存器資料持久化
- 網路透明閘道功能

# Modbus RTU Client 使用指南

## 功能概述

Client 端讓 MCU 以 **Modbus 主站 (Master)** 身份主動輪詢外部 RTU Sensor，並以非阻塞方式管理請求/回應、超時與例外狀態。實作架構採用 `modbus_rtu_client.c/.h` 與既有的 `uart_rs485_driver.c`，因此可沿用 RS485 DIR 控制與硬體 CRC 支援。

### 特色
- 支援 Function Code `0x03` 讀取保持暫存器、`0x04` 讀取輸入暫存器、`0x06` 寫入單一暫存器、`0x10` 寫入多個暫存器
- 自動計算預期回應長度並驗證 CRC16
- 偵測超時、異常回應與封包間隙
- 以狀態機回報成功、例外、逾時與錯誤

## 基本整合步驟

1. **包含標頭**
   ```c
   #ifdef MODBUS_RTU
   #include "modbus_rtu_client.h"
   #include "uart_rs485_driver.h"
   #endif
   ```

2. **初始化 UART/RS485 驅動**（與 Server 相同）
   ```c
   uart_rs485_driver_config_t uart_config = {
       .uart = UART0,
       .irq_number = UART0_IRQn,
       .module_clock = UART0_MODULE,
       .baudrate = 9600,
       .dir_gpio_port = PB,
       .dir_gpio_pin = 14,
       .timestamp_callback = modbus_get_timestamp_us,
       .timestamp_context = NULL
   };

   uart_rs485_driver_init(&uart_config);
   uart_rs485_driver_set_rx_callback(modbus_uart_rx_callback, &g_modbus_client);
   ```

3. **建立 Client 結構並初始化**
   ```c
   static modbus_rtu_client_t g_modbus_client;

   modbus_rtu_client_config_t client_config = {
       .tx_handler = modbus_uart_tx_write,   // 封裝 uart_rs485_driver_write
       .tx_context = NULL,
       .timestamp_callback = modbus_get_timestamp_us,
       .timestamp_context = NULL,
       .baudrate = 9600,
       .crc_method = MODBUS_CRC_METHOD_AUTO
   };

   bool ok = modbus_rtu_client_init(&g_modbus_client, &client_config);
   ```

4. **UART RX 回調轉交 Client 處理**
   ```c
   static void modbus_uart_rx_callback(uint8_t byte, uint32_t timestamp_us, void *context)
   {
       modbus_rtu_client_t *client = (modbus_rtu_client_t *)context;
       if (client != NULL)
       {
           modbus_rtu_client_handle_rx_byte(client, byte, timestamp_us);
       }
   }
   ```

5. **發起請求**
    ```c
    // 讀取保持暫存器 (0x03)
    modbus_rtu_client_start_read_holding(&g_modbus_client,
                                                     slave_address,
                                                     start_address,
                                                     quantity,
                                                     timeout_ms);

    // 讀取輸入暫存器 (0x04)
    modbus_rtu_client_start_read_input(&g_modbus_client,
                                                  slave_address,
                                                  start_address,
                                                  quantity,
                                                  timeout_ms);

    // 寫入單一暫存器 (0x06)
    modbus_rtu_client_start_write_single(&g_modbus_client,
                                                     slave_address,
                                                     register_address,
                                                     value,
                                                     timeout_ms);

    // 寫入多個暫存器 (0x10)
    modbus_rtu_client_start_write_multiple(&g_modbus_client,
                                                        slave_address,
                                                        start_address,
                                                        quantity,      // 最大 123 筆
                                                        values_buffer,
                                                        timeout_ms);
    ```

6. **在主迴圈輪詢狀態**
   ```c
   uint32_t now_us = g_systick_ms * 1000U;
   modbus_rtu_client_poll(&g_modbus_client, now_us);

   if (!modbus_rtu_client_is_busy(&g_modbus_client))
   {
       modbus_rtu_client_state_t state = modbus_rtu_client_get_state(&g_modbus_client);
       if (state == MODBUS_RTU_CLIENT_STATE_COMPLETE)
       {
        modbus_rtu_client_copy_response(&g_modbus_client, holding_buffer, buffer_len);
           modbus_rtu_client_clear(&g_modbus_client);
       }
       else if (state == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
       {
           modbus_exception_t exception = modbus_rtu_client_get_exception(&g_modbus_client);
           // TODO: 依需求處理例外
           modbus_rtu_client_clear(&g_modbus_client);
       }
       else if (state == MODBUS_RTU_CLIENT_STATE_TIMEOUT || state == MODBUS_RTU_CLIENT_STATE_ERROR)
       {
           // TODO: 觸發重試或錯誤指示
           modbus_rtu_client_clear(&g_modbus_client);
       }
   }
   ```

## 使用範例：四種 Client 功能碼完整示例

以下提供可直接嵌入的最小範例，展示如何以非阻塞啟動請求，並在狀態完成後讀取/列印結果。為簡化示範，下列函式各自封裝一個完整交易循環：

> 注意：示範中使用 `get_system_time_ms()` 提供毫秒時間，並在迴圈中呼叫 `modbus_rtu_client_poll()` 進行狀態推進；實務上建議放在你的主迴圈裡避免忙等。

### 0x03 讀取保持暫存器 (Read Holding Registers)

```c
bool demo_fc03_read_holding(modbus_rtu_client_t *cli,
                            uint8_t slave,
                            uint16_t start_address,
                            uint16_t quantity,
                            uint32_t timeout_ms)
{
    if (!modbus_rtu_client_start_read_holding(cli, slave, start_address, quantity, timeout_ms))
        return false;

    for (;;)
    {
        uint32_t now_us = get_system_time_ms() * 1000U;
        modbus_rtu_client_poll(cli, now_us);
        if (modbus_rtu_client_is_busy(cli))
            continue;

        modbus_rtu_client_state_t st = modbus_rtu_client_get_state(cli);
        if (st == MODBUS_RTU_CLIENT_STATE_COMPLETE)
        {
            uint16_t regs[MODBUS_RTU_CLIENT_MAX_REGISTERS];
            uint16_t n = modbus_rtu_client_copy_response(cli, regs, MODBUS_RTU_CLIENT_MAX_REGISTERS);
            printf("[MODBUS] FC03 read %u holding regs @0x%04X\r\n", (unsigned)n, start_address);
            for (uint16_t i = 0; i < n; ++i)
                printf("  [%04X] = 0x%04X\r\n", (unsigned)(start_address + i), regs[i]);
            modbus_rtu_client_clear(cli);
            return true;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            printf("[MODBUS] FC03 exception: 0x%02X\r\n", modbus_rtu_client_get_exception(cli));
            modbus_rtu_client_clear(cli);
            return false;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_TIMEOUT || st == MODBUS_RTU_CLIENT_STATE_ERROR)
        {
            printf("[MODBUS] FC03 timeout/error\r\n");
            modbus_rtu_client_clear(cli);
            return false;
        }
    }
}
```

### 0x04 讀取輸入暫存器 (Read Input Registers)

```c
bool demo_fc04_read_input(modbus_rtu_client_t *cli,
                          uint8_t slave,
                          uint16_t start_address,
                          uint16_t quantity,
                          uint32_t timeout_ms)
{
    if (!modbus_rtu_client_start_read_input(cli, slave, start_address, quantity, timeout_ms))
        return false;

    for (;;)
    {
        uint32_t now_us = get_system_time_ms() * 1000U;
        modbus_rtu_client_poll(cli, now_us);
        if (modbus_rtu_client_is_busy(cli))
            continue;

        modbus_rtu_client_state_t st = modbus_rtu_client_get_state(cli);
        if (st == MODBUS_RTU_CLIENT_STATE_COMPLETE)
        {
            uint16_t regs[MODBUS_RTU_CLIENT_MAX_REGISTERS];
            uint16_t n = modbus_rtu_client_copy_response(cli, regs, MODBUS_RTU_CLIENT_MAX_REGISTERS);
            printf("[MODBUS] FC04 read %u input regs @0x%04X\r\n", (unsigned)n, start_address);
            for (uint16_t i = 0; i < n; ++i)
                printf("  [%04X] = 0x%04X\r\n", (unsigned)(start_address + i), regs[i]);
            modbus_rtu_client_clear(cli);
            return true;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            printf("[MODBUS] FC04 exception: 0x%02X\r\n", modbus_rtu_client_get_exception(cli));
            modbus_rtu_client_clear(cli);
            return false;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_TIMEOUT || st == MODBUS_RTU_CLIENT_STATE_ERROR)
        {
            printf("[MODBUS] FC04 timeout/error\r\n");
            modbus_rtu_client_clear(cli);
            return false;
        }
    }
}
```

### 0x06 寫入單一暫存器 (Write Single Register)

```c
bool demo_fc06_write_single(modbus_rtu_client_t *cli,
                            uint8_t slave,
                            uint16_t reg_address,
                            uint16_t value,
                            uint32_t timeout_ms)
{
    if (!modbus_rtu_client_start_write_single(cli, slave, reg_address, value, timeout_ms))
        return false;

    for (;;)
    {
        uint32_t now_us = get_system_time_ms() * 1000U;
        modbus_rtu_client_poll(cli, now_us);
        if (modbus_rtu_client_is_busy(cli))
            continue;

        modbus_rtu_client_state_t st = modbus_rtu_client_get_state(cli);
        if (st == MODBUS_RTU_CLIENT_STATE_COMPLETE)
        {
            // FC06 回應會回 echo 地址/資料，內部已驗證成功
            printf("[MODBUS] FC06 wrote 0x%04X to [0x%04X]\r\n", value, reg_address);
            modbus_rtu_client_clear(cli);
            return true;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            printf("[MODBUS] FC06 exception: 0x%02X\r\n", modbus_rtu_client_get_exception(cli));
            modbus_rtu_client_clear(cli);
            return false;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_TIMEOUT || st == MODBUS_RTU_CLIENT_STATE_ERROR)
        {
            printf("[MODBUS] FC06 timeout/error\r\n");
            modbus_rtu_client_clear(cli);
            return false;
        }
    }
}
```

### 0x10 寫入多個暫存器 (Write Multiple Registers)

```c
bool demo_fc10_write_multiple(modbus_rtu_client_t *cli,
                              uint8_t slave,
                              uint16_t start_address,
                              uint16_t quantity,            // 最大 123
                              const uint16_t *values,
                              uint32_t timeout_ms)
{
    if (quantity == 0U || quantity > MODBUS_RTU_CLIENT_MAX_WRITE_MULTIPLE_REGISTERS)
        return false;

    if (!modbus_rtu_client_start_write_multiple(cli, slave, start_address, quantity, values, timeout_ms))
        return false;

    for (;;)
    {
        uint32_t now_us = get_system_time_ms() * 1000U;
        modbus_rtu_client_poll(cli, now_us);
        if (modbus_rtu_client_is_busy(cli))
            continue;

        modbus_rtu_client_state_t st = modbus_rtu_client_get_state(cli);
        if (st == MODBUS_RTU_CLIENT_STATE_COMPLETE)
        {
            // FC10 回應回 echo 起始位址與數量，內部已驗證
            printf("[MODBUS] FC10 wrote %u regs starting @0x%04X\r\n", (unsigned)quantity, start_address);
            modbus_rtu_client_clear(cli);
            return true;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_EXCEPTION)
        {
            printf("[MODBUS] FC10 exception: 0x%02X\r\n", modbus_rtu_client_get_exception(cli));
            modbus_rtu_client_clear(cli);
            return false;
        }
        else if (st == MODBUS_RTU_CLIENT_STATE_TIMEOUT || st == MODBUS_RTU_CLIENT_STATE_ERROR)
        {
            printf("[MODBUS] FC10 timeout/error\r\n");
            modbus_rtu_client_clear(cli);
            return false;
        }
    }
}
```

## 內建輪詢邏輯（Smart_Box 範例）

`Smart_Box/main.c` 中提供簡易輪詢器：

- 預設週期性發送 `0x04` 指令讀取輸入暫存器（可依需求改為 `0x03`）
- 將回應資料寫入 `g_modbus_sensor_registers[]`，並以 LED 閃爍顯示成功/失敗
- 連續失敗會累計 `g_modbus_consecutive_failures`，可作為錯誤指示依據

如需更換感測器：

```c
#define MODBUS_SENSOR_SLAVE_ADDRESS      0x05U   // 新的從站地址
#define MODBUS_SENSOR_START_ADDRESS      0x0010U
#define MODBUS_SENSOR_REGISTER_QUANTITY  6U
#define MODBUS_SENSOR_POLL_INTERVAL_MS   500U
#define MODBUS_SENSOR_RESPONSE_TIMEOUT_MS 150U
```

調整上列常數即可變更輪詢行為（地址、數量、輪詢頻率、逾時）。

### 常數與限制

- `MODBUS_RTU_CLIENT_MAX_REGISTERS`：單次讀取最多 125 筆暫存器
- `MODBUS_RTU_CLIENT_MAX_WRITE_MULTIPLE_REGISTERS`：單次寫入多筆時最多 123 筆 (遵循 ADU 256-byte 限制)
- 可使用之功能碼巨集定義於 `modbus_rtu_client.h`：
    - `MODBUS_RTU_FUNCTION_READ_HOLDING`
    - `MODBUS_RTU_FUNCTION_READ_INPUT`
    - `MODBUS_RTU_FUNCTION_WRITE_SINGLE`
    - `MODBUS_RTU_FUNCTION_WRITE_MULTIPLE`

## Client API 速覽

| 函數 | 說明 |
| ---- | ---- |
| `modbus_rtu_client_init` | 初始化 Client 結構與時序計算 |
| `modbus_rtu_client_start_read_holding` | 非阻塞發起 FC0x03 讀取請求 |
| `modbus_rtu_client_start_read_input` | 非阻塞發起 FC0x04 讀取請求 |
| `modbus_rtu_client_start_write_single` | 非阻塞發起 FC0x06 一筆寫入 |
| `modbus_rtu_client_start_write_multiple` | 非阻塞發起 FC0x10 多筆寫入 |
| `modbus_rtu_client_handle_rx_byte` | 於 UART RX IRQ 中逐 byte 餵入資料 |
| `modbus_rtu_client_poll` | 根據 timestamp 檢查超時與封包間隔 |
| `modbus_rtu_client_is_busy` | 確認是否仍等待回應 |
| `modbus_rtu_client_get_state` | 取得目前狀態（完成、例外、逾時、錯誤）|
| `modbus_rtu_client_copy_response` | 解析回應並複製為 16-bit 暫存器陣列 |
| `modbus_rtu_client_clear` | 重置狀態，允許下一筆請求 |

> **提示**：若現場需要同時支援 Client 與 Server，可保留 Server 程式碼並以條件編譯切換執行路徑。兩者共用 CRC、Timing 與 UART 驅動模組。