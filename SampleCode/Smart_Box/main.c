#include "NuMicro.h"
#include "ble_mesh_at.h"
#include <string.h>

#define PLL_CLOCK 192000000
#define PROJECT_NAME "Smart_Box"

// KeyA 按鍵設定 (PB.15)
#define KEY_A_LONG_PRESS_MS 5000u // 5 秒長按

// 全域變數
volatile uint32_t g_systick_ms = 0;

// KeyA 按鍵狀態 (PB.15 - active low)
static volatile bool g_key_a_pressed = false;
static volatile uint32_t g_key_a_press_start_ms = 0;
static volatile bool g_key_a_long_press_sent = false;

// BLE MESH AT 控制器
static ble_mesh_at_controller_t g_ble_at;
static volatile bool g_yellow_led_on = false;

// 簡易 LED 脈衝（無 printf 下的事件指示）：
// - 藍燈：收到一般行（LINE_RECEIVED）閃爍短脈衝
// - 紅燈：逾時/錯誤 閃爍較長脈衝
static volatile uint32_t g_red_on_until_ms = 0;
static volatile uint32_t g_blue_on_until_ms = 0;

// 黃燈快闃（MDTSG/MDTPG 訊息指示）
static volatile uint32_t g_yellow_flash_count = 0;
static volatile uint32_t g_yellow_flash_next_ms = 0;
static volatile bool g_yellow_flash_on = false;
#define YELLOW_FLASH_ON_MS 100u
#define YELLOW_FLASH_OFF_MS 150u

// 藍燈心跳（未綁定時每 2 秒閃 0.2 秒）
#define BLUE_HEARTBEAT_PERIOD_MS 2000u
#define BLUE_HEARTBEAT_ON_MS 200u
static volatile uint32_t g_blue_heartbeat_last_ms = 0;
static volatile bool g_provisioning_wait = false;

// 綁定與 UID 紀錄
static volatile bool g_is_bound = false;
static char g_device_uid[32];

// 最近一筆 Mesh 訊息解析結果
static char g_last_sender_uid[32];
static uint8_t g_last_payload[64];
static volatile uint32_t g_last_payload_len = 0;
static volatile uint32_t g_msg_count = 0;

// 取得系統時間函數
uint32_t get_system_time_ms(void)
{
    return g_systick_ms;
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

    // UART0 腳位設定
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

    SYS_LockReg();
}

// LED 腳位基礎初始化（不使用 LED 控制器，直接作為 GPIO 輸出）
static void leds_basic_init(void)
{
    GPIO_SetMode(PB, BIT1 | BIT2 | BIT3, GPIO_MODE_OUTPUT);
    // 預設全滅（LED 為 active-high）
    PB1 = 0;
    PB2 = 0;
    PB3 = 0;
}

// 黃燈（PB.2）控制
static inline void yellow_led_on(void)
{
    PB2 = 1;
}
static inline void yellow_led_off(void)
{
    PB2 = 0;
}

static inline void red_led_on(void) { PB1 = 1; }
static inline void red_led_off(void) { PB1 = 0; }
static inline void blue_led_on(void) { PB3 = 1; }
static inline void blue_led_off(void) { PB3 = 0; }

static inline void pulse_red(uint32_t duration_ms)
{
    red_led_on();
    g_red_on_until_ms = g_systick_ms + duration_ms;
}

static inline void pulse_blue(uint32_t duration_ms)
{
    blue_led_on();
    g_blue_on_until_ms = g_systick_ms + duration_ms;
}

// 黃燈快闃 N 次（例如 MDTSG=1, MDTPG=2）
static inline void flash_yellow(uint32_t count)
{
    if (g_yellow_flash_count > 0)
        return; // 還在闃耀中，忽略新要求
    g_yellow_flash_count = count;
    g_yellow_flash_next_ms = g_systick_ms + 10; // 立即開始
    g_yellow_flash_on = false;
}

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
    const char *p = hex;
    uint32_t n = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
    {
        if (hex_nibble(*p) < 0)
            return 0; // 非 hex 字元
        n++;
        p++;
    }
    if ((n & 1u) != 0u)
        return 0; // 必須是偶數長度
    uint32_t out_len = n / 2u;
    if (out_len > max_out)
        out_len = max_out;
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

static void handle_mesh_line(const char *line)
{
    // 複製到本地緩衝以便分詞
    char buf[128];
    uint32_t i = 0;
    while (line[i] && i < sizeof(buf) - 1)
    {
        buf[i] = line[i];
        i++;
    }
    buf[i] = '\0';

    // 檢查是否 MDTSG-MSG、MDTPG-MSG 或 MDTS-MSG
    const char *key1 = "MDTSG-MSG";
    const char *key2 = "MDTPG-MSG";
    const char *key3 = "MDTS-MSG";
    if (strstr(buf, key1) == NULL && strstr(buf, key2) == NULL && strstr(buf, key3) == NULL)
        return;

    // 以空白切分，取 tokens[1] 為 sender，最後一個 token 當作 hex payload
    char *tokens[8];
    int tcount = 0;
    char *tok = strtok(buf, " \t\r\n");
    while (tok && tcount < 8)
    {
        tokens[tcount++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }
    if (tcount < 3)
        return;
    const char *sender = tokens[1];
    const char *hex;
    if (strstr(tokens[0], key3) != NULL && tcount >= 3)
    {
        hex = tokens[3];
    }
    else
    {
        hex = tokens[tcount - 1];
    }

    // 儲存 sender
    uint32_t sl = 0;
    while (sender[sl] && sl < sizeof(g_last_sender_uid) - 1)
    {
        g_last_sender_uid[sl] = sender[sl];
        sl++;
    }
    g_last_sender_uid[sl] = '\0';

    // 轉換 hex 到 bytes
    g_last_payload_len = hex_to_bytes(hex, g_last_payload, (uint32_t)sizeof(g_last_payload));
    g_msg_count++;

    // 根據訊息類型觸發黃燈闃耀
    if (strstr(tokens[0], key1) != NULL)
        flash_yellow(1); // MDTSG 黃燈闃 1 次
    else if (strstr(tokens[0], key2) != NULL)
        flash_yellow(2); // MDTPG 黃燈闃 2 次
    else if (strstr(tokens[0], key3) != NULL)
        flash_yellow(3); // MDTS 黃燈闃 3 次
}

// BLE MESH AT 事件回調函數
void on_ble_mesh_at_event(ble_mesh_at_event_t event, const char *data)
{
    switch (event)
    {
    case BLE_MESH_AT_EVENT_VER_SUCCESS:
        if (!g_yellow_led_on)
        {
            yellow_led_on();
            g_yellow_led_on = true;
        }
        break;

    case BLE_MESH_AT_EVENT_REBOOT_SUCCESS:
        // 收到 REBOOT-MSG SUCCESS，藍燈短閃
        pulse_blue(120);
        break;

    case BLE_MESH_AT_EVENT_PROV_BOUND:
        // 綁定成功，藍燈持續亮
        blue_led_on();
        g_is_bound = true;
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
        // 停止藍燈心跳
        g_provisioning_wait = false;
        break;

    case BLE_MESH_AT_EVENT_PROV_UNBOUND:
        // 未綁定/解除綁定，關藍燈
        blue_led_off();
        g_is_bound = false;
        g_device_uid[0] = '\0';
        // 啟動藍燈心跳
        g_provisioning_wait = true;
        break;

    case BLE_MESH_AT_EVENT_LINE_RECEIVED:
        // 藍燈短脈衝表示有收到行（非 VER 成功）
        pulse_blue(120);
        // 嘗試解析 MDTSG/MDTPG
        handle_mesh_line(data);
        break;

    case BLE_MESH_AT_EVENT_TIMEOUT:
        // 紅燈長脈衝表示逾時
        pulse_red(500);
        break;

    case BLE_MESH_AT_EVENT_ERROR:
        // 紅燈長脈衝表示錯誤
        pulse_red(500);
        break;

    default:
        break;
    }
}

void UART1_IRQHandler(void)
{
    ble_mesh_at_uart_irq_handler(&g_ble_at);
}

// 數位 I/O 系統初始化
void digital_io_init(void)
{
    // PA.6 設定為輸出 (DO)
    GPIO_SetMode(PA, BIT6, GPIO_MODE_OUTPUT);
    PA6 = 0; // 初始輸出低電位

    // PB.7 設定為輸入 (DI)
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    // GPIO_SetPullCtl(PB, BIT7, GPIO_PUSEL_PULL_DOWN); // 設定下拉電阻

    // KeyA (PB.15) 設定為輸入 (active low)
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
    GPIO_SetPullCtl(PB, BIT15, GPIO_PUSEL_PULL_UP); // 設定上拉電阻
}

// 數位 I/O 測試函數
void digital_io_test(void)
{
    // 讀取 PB.7 輸入狀態
    bool di_state = (PB7 != 1);

    // 根據 PB.7 輸入控制 PA.6 輸出
    // PB.7 = 1 時，PA.6 = 0
    // PB.7 = 0 時，PA.6 = 1
    PA6 = di_state ? 1 : 0;
}

// KeyA 按鍵處理函數 (簡化版本)
void key_a_update(void)
{
    uint32_t current_time = g_systick_ms;
    bool key_pressed = (PB15 == 0); // active low

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
    else if (key_pressed && g_key_a_pressed && !g_key_a_long_press_sent)
    {
        // 按鍵持續按下，檢查是否達到長按時間
        uint32_t press_duration = current_time - g_key_a_press_start_ms;
        if (press_duration >= KEY_A_LONG_PRESS_MS)
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
    leds_basic_init();
    digital_io_init();

    // 設定 SysTick 為 1 毫秒精度
    // SystemCoreClock = 192MHz, 每毫秒需要 192000 個時鐘週期
    SysTick_Config(SystemCoreClock / 1000); // 1 毫秒中斷一次

    // 初始化 BLE MESH AT 模組
    ble_mesh_at_config_t ble_config = {
        .baudrate = 115200,
        .tx_pin_port = 0, // PA
        .tx_pin_num = 9,  // PA.9
        .rx_pin_port = 0, // PA
        .rx_pin_num = 8   // PA.8
    };
    ble_mesh_at_init(&g_ble_at, &ble_config, on_ble_mesh_at_event, get_system_time_ms);
    yellow_led_off();

    // 上電後先送 REBOOT，並進入等待綁定狀態（藍燈心跳）
    (void)ble_mesh_at_send_reboot(&g_ble_at);
    g_is_bound = false;
    g_device_uid[0] = '\0';
    g_provisioning_wait = true;
    g_blue_heartbeat_last_ms = g_systick_ms;

    // LED 測試序列關閉；改由 UART1 AT 驗證結果控制黃燈

    while (1)
    {
        uint32_t current_time = g_systick_ms;

        // 執行數位 I/O 測試
        digital_io_test();

        // 更新 KeyA 按鍵狀態
        key_a_update();

        // 更新 BLE MESH AT 模組
        ble_mesh_at_update(&g_ble_at);

        // 無阻塞 LED 脈衝維持：逾時到期自動熄滅
        if (g_red_on_until_ms && current_time >= g_red_on_until_ms)
        {
            red_led_off();
            g_red_on_until_ms = 0;
        }
        if (g_blue_on_until_ms && current_time >= g_blue_on_until_ms)
        {
            // 如果已綁定，保持藍燈常亮；否則依脈衝到期時間關閉
            if (!g_is_bound)
            {
                blue_led_off();
            }
            g_blue_on_until_ms = 0;
        }

        // 黃燈快閃狀態機（MDTSG/MDTPG 指示）
        if (g_yellow_flash_count > 0 && current_time >= g_yellow_flash_next_ms)
        {
            if (!g_yellow_flash_on)
            {
                // 開始一次閃爍
                yellow_led_on();
                g_yellow_flash_on = true;
                g_yellow_flash_next_ms = current_time + YELLOW_FLASH_ON_MS;
            }
            else
            {
                // 結束一次閃爍
                yellow_led_off();
                g_yellow_flash_on = false;
                g_yellow_flash_count--;
                if (g_yellow_flash_count > 0)
                {
                    g_yellow_flash_next_ms = current_time + YELLOW_FLASH_OFF_MS;
                }
            }
        }

        // 未綁定時的藍燈心跳（每 2 秒閃 0.2 秒）
        if (g_provisioning_wait && !g_is_bound)
        {
            if ((current_time - g_blue_heartbeat_last_ms) >= BLUE_HEARTBEAT_PERIOD_MS)
            {
                pulse_blue(BLUE_HEARTBEAT_ON_MS);
                g_blue_heartbeat_last_ms = current_time;
            }
        }

        // 持續事件處理與 LED 脈衝維持，不輸出文字
    }
}
