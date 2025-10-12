#ifndef BLE_MESH_AT_H
#define BLE_MESH_AT_H

#include <stdint.h>
#include <stdbool.h>

// BLE MESH AT 模組配置
typedef struct
{
    uint32_t baudrate;   // UART 波特率
    uint8_t tx_pin_port; // TX 腳位 port (如 PA = 0, PB = 1...)
    uint8_t tx_pin_num;  // TX 腳位編號
    uint8_t rx_pin_port; // RX 腳位 port
    uint8_t rx_pin_num;  // RX 腳位編號
} ble_mesh_at_config_t;

// BLE MESH AT 事件類型
typedef enum
{
    BLE_MESH_AT_EVENT_NONE = 0,
    BLE_MESH_AT_EVENT_VER_SUCCESS,    // VER-MSG SUCCESS 回應
    BLE_MESH_AT_EVENT_VER_FAILED,     // VER 命令失敗
    BLE_MESH_AT_EVENT_LINE_RECEIVED,  // 接收到一般回應行
    BLE_MESH_AT_EVENT_TIMEOUT,        // 命令逾時
    BLE_MESH_AT_EVENT_ERROR,          // 通訊錯誤
    BLE_MESH_AT_EVENT_REBOOT_SUCCESS, // 收到 REBOOT-MSG SUCCESS
    BLE_MESH_AT_EVENT_PROV_BOUND,     // 收到 SYS-MSG DEVICE PROV-ED / PROV-MSG SUCCESS
    BLE_MESH_AT_EVENT_PROV_UNBOUND    // 收到 SYS-MSG DEVICE UNPROV
} ble_mesh_at_event_t;

// BLE MESH AT 狀態
typedef enum
{
    BLE_MESH_AT_STATE_IDLE = 0,
    BLE_MESH_AT_STATE_SENDING,
    BLE_MESH_AT_STATE_WAITING_RESPONSE,
    BLE_MESH_AT_STATE_VER_OK,
    BLE_MESH_AT_STATE_ERROR
} ble_mesh_at_state_t;

// 除錯與診斷欄位（無 printf 環境可於偵錯器/中斷暫停觀察）
typedef struct
{
    volatile uint32_t irq_count;       // UART1 IRQ 次數
    volatile uint32_t rx_byte_count;   // 累計接收位元組數
    volatile uint32_t line_count;      // 已完成行數
    volatile uint32_t drop_byte_count; // 因上一行未處理而丟棄的位元組
    volatile uint32_t timeout_count;   // 命令逾時計數
    volatile uint32_t error_count;     // 錯誤計數
    volatile uint32_t tx_count;        // 傳送命令次數

    volatile uint32_t last_intsts;  // 最近一次 INTSTS 快照
    volatile uint32_t last_fifosts; // 最近一次 FIFOSTS 快照

    volatile uint32_t last_line_len; // 最近一行長度
    ble_mesh_at_event_t last_event;  // 最近一次事件
    char last_line[128];             // 最近一行內容快照（以 NUL 結尾）
    char last_cmd[32];               // 最近送出的命令（最多 31 字元）
} ble_mesh_at_diag_t;

// BLE MESH AT 控制器結構
typedef struct
{
    // 配置
    ble_mesh_at_config_t config;

    // 狀態
    ble_mesh_at_state_t state;
    bool initialized;

    // 接收緩衝
    volatile bool line_ready;
    char rx_line[128];
    volatile uint32_t rx_line_len;
    volatile bool seen_cr;

    // 命令管理
    bool ver_sent;
    uint32_t last_command_time;
    uint32_t response_timeout_ms;

    // 回調函數指標
    void (*event_callback)(ble_mesh_at_event_t event, const char *data);
    uint32_t (*get_time_ms)(void);

    // 診斷欄位（可用以設斷點或即時觀察）
    ble_mesh_at_diag_t diag;

    // 裝置綁定狀態（根據 SYS-MSG/PROV-MSG）
    bool is_bound;
    char uid[32]; // 最近一次回報的 UID（例如 0x0028 等）
} ble_mesh_at_controller_t;

// 初始化函數
void ble_mesh_at_init(ble_mesh_at_controller_t *controller,
                      const ble_mesh_at_config_t *config,
                      void (*event_callback)(ble_mesh_at_event_t, const char *),
                      uint32_t (*get_time_ms)(void));

// 更新函數（在主迴圈中調用）
void ble_mesh_at_update(ble_mesh_at_controller_t *controller);

// 命令發送函數
bool ble_mesh_at_send_ver(ble_mesh_at_controller_t *controller);
bool ble_mesh_at_send_command(ble_mesh_at_controller_t *controller, const char *command);
bool ble_mesh_at_send_reboot(ble_mesh_at_controller_t *controller);
bool ble_mesh_at_send_nr(ble_mesh_at_controller_t *controller);

// 狀態查詢函數
ble_mesh_at_state_t ble_mesh_at_get_state(const ble_mesh_at_controller_t *controller);
bool ble_mesh_at_is_ready(const ble_mesh_at_controller_t *controller);
bool ble_mesh_at_is_ver_ok(const ble_mesh_at_controller_t *controller);
static inline bool ble_mesh_at_is_bound(const ble_mesh_at_controller_t *c) { return c && c->initialized ? c->is_bound : false; }
static inline const char *ble_mesh_at_get_uid(const ble_mesh_at_controller_t *c) { return c && c->initialized ? c->uid : ""; }

// 內部使用的 UART1 中斷處理函數（需要在 main.c 中的 UART1_IRQHandler 調用）
void ble_mesh_at_uart_irq_handler(ble_mesh_at_controller_t *controller);

#endif // BLE_MESH_AT_H