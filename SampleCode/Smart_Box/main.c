#include "NuMicro.h"
#include "ble_mesh_at.h"
#include "led_indicator.h"
#include "digital_io.h"
#include "mesh_handler.h"
#include <string.h>

#ifdef MODBUS_RTU
#include "modbus_sensor_manager.h"
#include "mesh_modbus_agent.h"
#endif

#define PLL_CLOCK 192000000
#define PROJECT_NAME "Smart_Box"

// KeyA 按鍵設定 (PB.15)
#define KEY_A_LONG_PRESS_MS 5000u // 5 秒長按

#ifdef MODBUS_RTU
#define MODBUS_SENSOR_SLAVE_ADDRESS (0x01U)
#define MODBUS_SENSOR_START_ADDRESS (0x0000U)
#define MODBUS_SENSOR_REGISTER_QUANTITY (6U)
#define MODBUS_SENSOR_POLL_INTERVAL_MS (1000U)
#define MODBUS_SENSOR_RESPONSE_TIMEOUT_MS (200U)
#define MODBUS_SENSOR_FAILURE_THRESHOLD (3U)
#endif

// 全域變數
volatile uint32_t g_systick_ms = 0;

#ifdef MODBUS_RTU
static modbus_sensor_manager_t g_modbus_sensor_manager;
static mesh_modbus_agent_t g_mesh_modbus_agent;
#endif

// KeyA 按鍵狀態 (PB.15 - active low)
static volatile bool g_key_a_pressed = false;
static volatile uint32_t g_key_a_press_start_ms = 0;
static volatile bool g_key_a_long_press_sent = false;
// 按鍵去抖
static volatile uint32_t g_key_a_last_change_ms = 0;
#define KEY_DEBOUNCE_MS 30u

// ========================= BLE Mesh 與 LED/Mesh 指示區 =========================
// BLE MESH AT 控制器
static ble_mesh_at_controller_t g_ble_at;
// 由 led_indicator 模組接管所有 LED 時序/閃爍管理
// 僅保留綁定旗標供其他模組（例如 mesh handler）使用（可視需求後續移除）
// 綁定 UID 紀錄（如需可保留）
static char g_device_uid[32];

// 最近一筆 Mesh 訊息解析狀態（MDTS/MDTSG/MDTPG 共用）
// Mesh 訊息解析交由 mesh_handler 處理

// PA.6 由 Mesh 指令控制的自動關閉計時
// 規格：收到 0x31(ON) 後啟動，5 秒內再收 0x31 則自動延長 5 秒；收到 0x30(OFF) 立即關閉並清除計時
#define PA6_ON_HOLD_MS 5000u
static volatile uint32_t g_pa6_auto_off_deadline_ms = 0;
// ==============================================================================

// 取得系統時間函數
uint32_t get_system_time_ms(void)
{
    return g_systick_ms;
}

// KeyA 長按回調函數：發送解綁命令
static void send_nr_command(void)
{
    (void)ble_mesh_at_send_nr(&g_ble_at);
}

// SysTick 中斷處理函數
void SysTick_Handler(void)
{
    g_systick_ms++;
}

void SYS_Init(void)
{
    SYS_UnlockReg();
    PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);
    CLK_SetCoreClock(PLL_CLOCK);
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);

    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HXT, CLK_CLKDIV0_UART0(1));
    SystemCoreClockUpdate();

#ifdef MODBUS_RTU
    // UART0 腳位設定 (PD2=RXD, PD3=TXD for MODBUS RTU)
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD2MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD2MFP_UART0_RXD | SYS_GPD_MFPL_PD3MFP_UART0_TXD);
    SYS->GPB_MFPH &= ~SYS_GPB_MFPH_PB14MFP_Msk;
    SYS->GPB_MFPH |= SYS_GPB_MFPH_PB14MFP_GPIO;
#else
    // UART0 腳位設定 (PB12=RXD, PB13=TXD for console)
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);
#endif

    SYS_LockReg();
}

// 原始 LED 腳位初始化改由 led_indicator_init() 內部完成

// 小工具：十六進位字串轉 bytes（僅接受 [0-9A-Fa-f]，長度需為偶數）
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return 10 + (int)(c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (int)(c - 'A');
    return -1;
}

static uint32_t hex_to_bytes(const char *hex, uint8_t *out, uint32_t max_out)
{
    // 跳過前導空白
    while (*hex == ' ' || *hex == '\t')
        hex++;

    // 找到有效hex字串的結尾
    const char *p = hex;
    uint32_t n = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
    {
        if (hex_nibble(*p) < 0)
            return 0; // 非 hex 字元
        n++;
        p++;
    }

    // 檢查長度：必須>0且為偶數
    if (n == 0 || (n & 1u) != 0u)
        return 0;

    uint32_t out_len = n / 2u;
    if (out_len > max_out)
        out_len = max_out;

    // 轉換hex到bytes
    for (uint32_t i = 0; i < out_len; i++)
    {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return out_len;
}

// Mesh 訊息解析交由 mesh_handler 處理

// BLE MESH AT 事件回調函數
void on_ble_mesh_at_event(ble_mesh_at_event_t event, const char *data)
{
    // 將事件傳遞給 mesh handler 處理
    mesh_handler_event(event, data);

    // 保留一些主程式需要的特殊處理
    switch (event)
    {
    case BLE_MESH_AT_EVENT_PROV_BOUND:
        // 記錄 UID
        {
            const char *uid = ble_mesh_at_get_uid(&g_ble_at);
            size_t k = 0;
            while (uid[k] && k < sizeof(g_device_uid) - 1)
            {
                g_device_uid[k] = uid[k];
                k++;
            }
            g_device_uid[k] = '\0';
        }
        // LED 狀態交由 led_indicator 控管
        led_set_binding_state(true);
        led_set_provisioning_wait(false);
        break;

    case BLE_MESH_AT_EVENT_PROV_UNBOUND:
        g_device_uid[0] = '\0';
        led_set_binding_state(false);
        led_set_provisioning_wait(true);
        break;

    default:
        break;
    }
}

void UART1_IRQHandler(void)
{
    ble_mesh_at_uart_irq_handler(&g_ble_at);
}

#ifdef MODBUS_RTU
// MODBUS Sensor Manager 回調函數
static void modbus_sensor_success_callback(const uint16_t *registers, uint16_t quantity);
static void modbus_sensor_error_callback(modbus_exception_t exception, uint32_t consecutive_failures);

// MESH MODBUS Agent 回調函數
static void agent_response_ready_callback(const uint8_t *data, uint8_t length);
static void agent_error_callback(uint8_t error_code);
static void agent_mesh_data_callback(const uint8_t *data, uint8_t length);

static void modbus_sensor_success_callback(const uint16_t *registers, uint16_t quantity)
{
    (void)registers;
    (void)quantity;
    led_pulse_blue(120);
}

static void modbus_sensor_error_callback(modbus_exception_t exception, uint32_t consecutive_failures)
{
    (void)exception;
    (void)consecutive_failures;
    led_pulse_red(200);
}

// MESH MODBUS Agent 回調函數實作

// Agent 回應準備好，轉成 Hex String 並透過 BLE Mesh 回傳
static void agent_response_ready_callback(const uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0)
    {
        return;
    }

    // 將 bytes 轉成 Hex String
    char hex_string[80]; // 最多 40 bytes * 2 = 80 chars
    uint8_t hex_idx = 0;

    for (uint8_t i = 0; i < length && hex_idx < sizeof(hex_string) - 2; i++)
    {
        uint8_t b = data[i];
        uint8_t hi = (b >> 4) & 0x0F;
        uint8_t lo = b & 0x0F;

        // 高 4 位
        if (hi < 10)
            hex_string[hex_idx++] = '0' + hi;
        else
            hex_string[hex_idx++] = 'A' + (hi - 10);

        // 低 4 位
        if (lo < 10)
            hex_string[hex_idx++] = '0' + lo;
        else
            hex_string[hex_idx++] = 'A' + (lo - 10);
    }
    hex_string[hex_idx] = '\0';

    // 透過 BLE Mesh AT 傳送 MDTS 訊息
    // 組裝 AT+MDTS=<hex_string> 命令
    char mdts_cmd[96]; // "AT+MDTS=" + 80 chars + null
    uint8_t cmd_idx = 0;
    const char *prefix = "AT+MDTS=";
    while (*prefix && cmd_idx < sizeof(mdts_cmd) - 1)
    {
        mdts_cmd[cmd_idx++] = *prefix++;
    }
    uint8_t hex_pos = 0;
    while (hex_string[hex_pos] && cmd_idx < sizeof(mdts_cmd) - 1)
    {
        mdts_cmd[cmd_idx++] = hex_string[hex_pos++];
    }
    mdts_cmd[cmd_idx] = '\0';
    ble_mesh_at_send_command(&g_ble_at, mdts_cmd);

    // 閃藍燈一次表示回應成功
    led_pulse_blue(120);
}

static void agent_error_callback(uint8_t error_code)
{
    (void)error_code;
    // 錯誤處理已在 agent 內部完成（閃紅燈）
}

// 從 Mesh Handler 接收到 Agent 格式的資料
static void agent_mesh_data_callback(const uint8_t *data, uint8_t length)
{
    // 轉發給 Agent 處理
    mesh_modbus_agent_process_mesh_data(&g_mesh_modbus_agent, data, length);
}
#endif

// Mesh Handler 回調函數實作
static void set_binding_state(bool bound)
{
    led_set_binding_state(bound);
}

static void flash_yellow_led(uint32_t count)
{
    led_flash_yellow(count);
}

static void set_pa6_output(bool state)
{
    if (state)
    {
        PA6 = 1;
        // 設定 30 秒後自動關閉
        g_pa6_auto_off_deadline_ms = g_systick_ms + (30U * 1000U);
    }
    else
    {
        PA6 = 0;
        g_pa6_auto_off_deadline_ms = 0;
    }
}

// LED 控制適配器函數
static void set_yellow_led(bool on)
{
    led_set_yellow_status(on);
}

// KeyA 按鍵處理函數 (加入去抖功能)
void key_a_update(void)
{
    uint32_t current_time = g_systick_ms;
    bool key_pressed = (PB15 == 0); // active low

    // 檢查按鍵狀態是否改變
    bool state_changed = (key_pressed != g_key_a_pressed);

    if (state_changed)
    {
        // 狀態改變，檢查去抖時間
        if ((int32_t)(current_time - g_key_a_last_change_ms - KEY_DEBOUNCE_MS) >= 0)
        {
            // 去抖時間已過，接受狀態改變
            g_key_a_last_change_ms = current_time;

            if (key_pressed && !g_key_a_pressed)
            {
                // 按鍵剛被按下
                g_key_a_pressed = true;
                g_key_a_press_start_ms = current_time;
                g_key_a_long_press_sent = false;
            }
            else if (!key_pressed && g_key_a_pressed)
            {
                // 按鍵剛被釋放
                g_key_a_pressed = false;
                g_key_a_long_press_sent = false;
            }
        }
        // 否則忽略此次狀態改變（在去抖期間內）
    }
    else if (key_pressed && g_key_a_pressed && !g_key_a_long_press_sent)
    {
        // 按鍵持續按下，檢查是否達到長按時間
        if ((int32_t)(current_time - g_key_a_press_start_ms - KEY_A_LONG_PRESS_MS) >= 0)
        {
            // 長按 5 秒，發送 AT+NR 解綁命令
            (void)ble_mesh_at_send_nr(&g_ble_at);
            g_key_a_long_press_sent = true; // 防止重複發送
        }
    }
}

int main()
{
    SYS_Init();
    digital_io_init();

    // 設定 KeyA 長按回調 (5秒長按發送解綁命令)
    digital_io_set_key_callback(send_nr_command);

    // 初始化 LED 指示器
    led_indicator_init();

    // 初始化 Mesh Handler 回調
    mesh_handler_callbacks_t mesh_callbacks = {
        .led_yellow = set_yellow_led,
        .led_pulse_blue = led_pulse_blue,
        .led_pulse_red = led_pulse_red,
        .led_binding = set_binding_state,
        .led_flash = flash_yellow_led,
        .pa6_control = set_pa6_output,
#ifdef MODBUS_RTU
        .agent_response = agent_mesh_data_callback
#else
        .agent_response = NULL
#endif
    };
    mesh_handler_init(&mesh_callbacks);

    // 設定 SysTick 為 1 毫秒精度
    SysTick_Config(SystemCoreClock / 1000); // 1 毫秒中斷一次

#ifdef MODBUS_RTU
    // 直接初始化 MODBUS Sensor Manager
    modbus_sensor_config_t sensor_config = {
        .slave_address = MODBUS_SENSOR_SLAVE_ADDRESS,
        .start_address = MODBUS_SENSOR_START_ADDRESS,
        .register_quantity = MODBUS_SENSOR_REGISTER_QUANTITY,
        .poll_interval_ms = MODBUS_SENSOR_POLL_INTERVAL_MS,
        .response_timeout_ms = MODBUS_SENSOR_RESPONSE_TIMEOUT_MS,
        .failure_threshold = MODBUS_SENSOR_FAILURE_THRESHOLD};

    if (!modbus_sensor_manager_init(&g_modbus_sensor_manager, &sensor_config,
                                    modbus_sensor_success_callback,
                                    modbus_sensor_error_callback))
    {
        led_red_on();
    }

    // 初始化 MESH MODBUS Agent
    mesh_modbus_agent_config_t agent_config = {
        .mode = MESH_MODBUS_AGENT_MODE_RL, // 預設使用 RL Mode
        .modbus_timeout_ms = 500,          // MODBUS 超時 500ms
        .max_response_wait_ms = 1000       // 最大等待 1 秒
    };

    if (!mesh_modbus_agent_init(&g_mesh_modbus_agent, &agent_config,
                                &g_modbus_sensor_manager.client,
                                agent_response_ready_callback,
                                agent_error_callback,
                                led_flash_yellow))
    {
        led_red_on();
    }
#endif

    // 初始化 BLE MESH AT 模組
    ble_mesh_at_config_t ble_config = {
        .baudrate = 115200,
        .tx_pin_port = 0, // PA
        .tx_pin_num = 9,  // PA.9
        .rx_pin_port = 0, // PA
        .rx_pin_num = 8   // PA.8
    };
    ble_mesh_at_init(&g_ble_at, &ble_config, on_ble_mesh_at_event, get_system_time_ms);
    led_yellow_off();

    // 上電後先送 REBOOT，並進入等待綁定狀態（藍燈心跳）
    (void)ble_mesh_at_send_reboot(&g_ble_at);
    g_device_uid[0] = '\0';
    led_set_binding_state(false);
    led_set_provisioning_wait(true);

    while (1)
    {
        uint32_t current_time = g_systick_ms;

        digital_io_update(current_time);

        led_indicator_update(current_time);

#ifdef MODBUS_RTU
        modbus_sensor_manager_poll(&g_modbus_sensor_manager, current_time);
        mesh_modbus_agent_poll(&g_mesh_modbus_agent, current_time);
#endif

        // 更新 BLE MESH AT 模組
        ble_mesh_at_update(&g_ble_at);

        // PA6 Mesh ON 保持計時：逾時則自動關閉
        if (g_pa6_auto_off_deadline_ms && (int32_t)(current_time - g_pa6_auto_off_deadline_ms) >= 0)
        {
            PA6 = 0; // 自動 OFF
            g_pa6_auto_off_deadline_ms = 0;
        }
    }
}
