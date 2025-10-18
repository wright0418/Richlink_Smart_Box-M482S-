## Smart_Box — Mesh Modbus 與 BLE Mesh AT 使用手冊 (繁體中文)

本文件說明如何在本專案的 Smart_Box 範例中使用以下三個核心元件：

- `mesh_modbus_agent`：負責把 Mesh 網路上的請求轉為 Modbus RTU 請求並回傳結果。
- Modbus RTU client（範例中由 `modbus_sensor_manager` 提供）：與 Modbus RTU 裝置通訊（透過 UART0 + RS485）。
- `ble_mesh_at`：管理 BLE Mesh AT 模組（UART 通訊、AT 指令交互與事件回調）。

範例檔案：`SampleCode/Smart_Box/main.c` 已展示整合範例，建議先閱讀該檔以了解整體流程。

---

## 一、系統與硬體前提

- MCU 時脈、UART 與 GPIO 初始化由 `sys_init()` 處理（見 `main.c`）。範例中：
  - 使用 `UART0`（PD.2 = RXD, PD.3 = TXD）做 Modbus RTU（RS485）。
  - RS485 向列控制腳位：PB.14 設為 GPIO（用於 DE/RE）。
  - SysTick 設為 1ms tick（`SysTick_Config(SystemCoreClock / 1000)`）。

- 請確認硬體接線：
  - UART0 TX/RX 連接到 RS485 transceiver 的 DI/RO。
  - RS485 transceiver 的 DE/RE 由 PB.14 控制（範例依專案驅動設定此邏輯）。

---

## 二、初始化流程（程式碼片段與說明）

1) Modbus RTU client（以 `modbus_sensor_manager` 為例）

```c
modbus_sensor_config_t sensor_config = {
    .slave_address = MODBUS_SENSOR_SLAVE_ADDRESS,
    .start_address = MODBUS_SENSOR_START_ADDRESS,
    .register_quantity = MODBUS_SENSOR_REGISTER_QUANTITY,
    .poll_interval_ms = MODBUS_SENSOR_POLL_INTERVAL_MS,
    .response_timeout_ms = MODBUS_SENSOR_RESPONSE_TIMEOUT_MS,
    .failure_threshold = MODBUS_SENSOR_FAILURE_THRESHOLD
};

modbus_sensor_manager_t manager;
modbus_sensor_manager_init(&manager, &sensor_config,
                          modbus_sensor_success_callback,
                          modbus_sensor_error_callback);
```

- 說明：此 manager 會依照 `poll_interval_ms` 週期發出讀取請求，當取得成功或失敗時會呼叫你提供的 callback。

Callback 範例：

```c
static void modbus_sensor_success_callback(const uint16_t *registers, uint16_t quantity)
{
    // registers 指向回傳的暫存資料（quantity 個 16-bit registers）
}

static void modbus_sensor_error_callback(modbus_exception_t exception, uint32_t consecutive_failures)
{
    // exception：Modbus 例外或錯誤類型
    // consecutive_failures：連續失敗次數，可用於重試或切換備援
}
```

2) mesh_modbus_agent

```c
mesh_modbus_agent_config_t agent_config = {
    .mode = MESH_MODBUS_AGENT_MODE_RL,
    .modbus_timeout_ms = 300,
    .max_response_wait_ms = 500
};

mesh_modbus_agent_t agent;
mesh_modbus_agent_init(&agent, &agent_config,
                       &manager.client,
                       agent_response_ready_callback,
                       agent_error_callback,
                       NULL /* optional context */);
```

- 說明：
  - `mode` 決定 Agent 的行為，範例使用 `MESH_MODBUS_AGENT_MODE_RL`（請參考 header 列舉定義）。
  - `&manager.client` 將 Modbus 客戶端注入給 agent，以便 agent 發送 RTU 請求。

Agent callback 範例：

```c
static void agent_response_ready_callback(const uint8_t *data, uint8_t length)
{
    // 當 Modbus 回應準備好時被呼叫
    // 範例中會將 bytes 轉為 hex 字串，再透過 ble_mesh_at 發送 AT+MDTS
}

static void agent_error_callback(uint8_t error_code)
{
    // 錯誤處理（如超時、格式錯誤）
}
```

3) `ble_mesh_at` 初始化與事件處理

```c
ble_mesh_at_config_t ble_config = {
    .baudrate = 115200,
    .tx_pin_port = 0, // PA
    .tx_pin_num = 9,  // PA.9
    .rx_pin_port = 0, // PA
    .rx_pin_num = 8   // PA.8
};

ble_mesh_at_controller_t ble;
ble_mesh_at_init(&ble, &ble_config, on_ble_mesh_at_event, get_system_time_ms);

// 範例事件 callback
static void on_ble_mesh_at_event(ble_mesh_at_event_t event, const char *data)
{
    // 可處理配對、連線、AT 指令結果等事件，範例中將事件轉發給 mesh_handler
}
```

4) 主迴圈中需呼叫的函式

在 main loop 中請確保以下被週期性呼叫（範例中以 SysTick / g_systick_ms 控制）：

- `digital_io_update(current_time);`
- `led_indicator_update(current_time);`
- `mesh_modbus_agent_poll(&agent, current_time);`  // 觸發 agent 狀態機與逾時計時
- `ble_mesh_at_update(&ble);`                    // 處理 UART 與 AT 事件

另外：當 mesh 接收到網路請求時，會呼叫你在 `mesh_handler_init()` 中註冊的 `agent_mesh_data_callback`，該 callback 應呼叫：

```c
mesh_modbus_agent_process_mesh_data(&agent, data, length);
```

若 agent 忙碌，範例中示範把請求緩衝在 `mesh_handler` 的 pending 區，並以 `process_pending_agent_request()` 在空閒時取出處理。

---

## 三、訊息流程 (Request/Response lifecycle)

1. Mesh 節點送出請求（如透過 AT+MDTS 或其他 mesh 封包）。
2. `mesh_handler` 接收封包並呼叫 `agent_mesh_data_callback`。
3. `agent_mesh_data_callback` 呼 `mesh_modbus_agent_process_mesh_data()`：如果請求可處理，agent 轉為發送 Modbus RTU 請求。
4. Modbus RTU client 發送資料到 RS485，並等待 `response_timeout_ms`。
5. 成功取得 Modbus RTU 回應後，agent 呼 `agent_response_ready_callback`，使用者轉換回 Mesh/AT 格式並回傳來源節點。
6. 若失敗或逾時，agent 呼 `agent_error_callback`，可決定回傳錯誤碼或丟棄。

---

## 四、常見整合細節與注意事項

- RS485 半雙工控制：發送期間必須將 DE/RE 設為輸出（使能驅動），發送完成後再切回接收。範例使用 PB.14 作為控制腳位；請確認驅動程式在發送前後正確切換該腳位。
- 時序設定：`modbus_timeout_ms` 與 `max_response_wait_ms` 需依從設備回應速度調整；若頻繁逾時，提升 timeout 或降低 poll rate。
- 緩衝區大小：若 mesh 上可能同時來大量請求，請檢視 `mesh_handler` 的 pending buffer 大小與行為（範例中會選擇丟棄或累計統計）。

---

## 五、測試指南

1) 硬體自測

- 使用 PC 或 USB-to-RS485 工具模擬 Modbus slave，設定相同 slave address 與傳輸參數（9600/8/N/1 或範例中設定）。
- 用簡單的 Modbus 診斷工具（如 mbpoll、QModMaster）確認 MCU 發出的請求與 slave 回應正確。

2) Mesh 層測試

- 使用 BLE Mesh AT 模組的 AT 指令發送測試封包；範例把回應包用 AT+MDTS 送出給 Mesh。可在模組端或其他節點監聽回傳。

3) Log 與 LED 診斷

- 範例中若初始化失敗會呼 `led_red_on()`，可用於快速判斷初始化階段錯誤。
- 如需更多序列埠輸出，可於專案中啟用 semihost 或 `retarget.c` 支援並在 callback 中打印詳細錯誤。

---

## 六、除錯建議與常見問題

- 問：Modbus 一直逾時或沒有回應？
  - 檢查 RS485 DE/RE 控制腳位（PB.14）是否在傳送時啟用。
  - 確認 UART0 的 TX/RX 與 transceiver連線無誤 (PD.2/PD.3)。
  - 檢查 Modbus slave address、波特率與 parity 設定是否一致。

- 問：Agent 沒有收到 Mesh 的請求？
  - 確認 `mesh_handler_init()` 的 `agent_response` callback 已指向 `agent_mesh_data_callback`。
  - 確認 `ble_mesh_at` 已初始化，且 `ble_mesh_at_update()` 每循環呼叫以處理 UART 事件。

- 問：Mesh 回應被截斷或格式錯誤？
  - 確認 `bytes_to_hex()` 與 AT+MDTS 命令格式正確；若資料長度大，注意 AT 指令的最大長度限制。

---

## 七、如何在此專案延伸或修改

- 新增支援更多 Modbus 功能碼（Read Coils/Write Single/Register）時：在 `mesh_modbus_agent` 中擴展 `mesh_modbus_agent_process_mesh_data()` 的解析邏輯，並讓 agent 可以依需求呼叫 `manager.client` 的不同 API。
- 若要支援多個 Modbus 後端（例如同時有溫濕度與電力模組），可以在 `modbus_sensor_manager` 外再包一層 manager map，根據不同 mesh 請求路由到不同的 `modbus_sensor_manager` 或 client instance。

---

## 八、檔案與符號參考（在本專案中）

- 範例整合檔：`SampleCode/Smart_Box/main.c`（初始化、callback 與主迴圈展示）。
- 相關 Header：
  - `ble_mesh_at.h`（BLE Mesh AT controller API 與事件定義）
  - `mesh_modbus_agent.h`（Agent API）
  - `modbus_sensor_manager.h`（Modbus client manager API）

若需閱讀 API 詳細定義，請打開上述 header 與對應的 source 檔案。

---

如果你希望我：
- 產生一份更精簡的快速上手範例（只包含最小化的 main.c 與設定），或
- 幫你在專案中加入更完整的 debug log（例如透過 UART 打印狀態），

我可以直接修改專案與新增測試程式碼 — 告訴我你想優先的項目。

---

作者：自動產生文件（參考 `SampleCode/Smart_Box/main.c`）
日期：2025-10-18
