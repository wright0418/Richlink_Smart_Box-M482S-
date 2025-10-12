#include "NuMicro.h"
#include "led_controller.h"
#include "key_controller.h"
#include "ble_mesh_at.h"

#define PLL_CLOCK 192000000
#define PROJECT_NAME "Smart_Box"

// LED 索引定義
#define LED_RED_INDEX 0
#define LED_YELLOW_INDEX 1
#define LED_BLUE_INDEX 2
#define LED_COUNT 3

// 按鍵索引定義
#define KEY_A_INDEX 0
#define KEY_COUNT 1

// LED 顯示模式
typedef enum
{
    LED_MODE_NORMAL = 0, // 正常閃爍模式
    LED_MODE_SLOW,       // 慢速閃爍模式
    LED_MODE_FAST,       // 快速閃爍模式
    LED_MODE_WAVE,       // 流水燈模式
    LED_MODE_COUNT       // 模式總數
} led_display_mode_t;

// 全域變數
volatile uint32_t g_systick_ms = 0;

// LED 控制器陣列
led_controller_t led_controllers[LED_COUNT];
led_manager_t led_manager;

// 按鍵控制器陣列
key_controller_t key_controllers[KEY_COUNT];
key_manager_t key_manager;

// LED 顯示模式
volatile led_display_mode_t current_led_mode = LED_MODE_NORMAL;

// 控制是否啟用原本的 LED 管理（預設關閉，改由 UART 測試控制黃燈）
static bool g_led_control_enabled = false;

// BLE MESH AT 控制器
static ble_mesh_at_controller_t g_ble_at;
static volatile bool g_yellow_led_on = false;

// 簡易 LED 脈衝（無 printf 下的事件指示）：
// - 藍燈：收到一般行（LINE_RECEIVED）閃爍短脈衝
// - 紅燈：逾時/錯誤 閃爍較長脈衝
static volatile uint32_t g_red_on_until_ms = 0;
static volatile uint32_t g_blue_on_until_ms = 0;

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
        // 綁定成功，亮黃燈
        yellow_led_on();
        g_yellow_led_on = true;
        break;

    case BLE_MESH_AT_EVENT_PROV_UNBOUND:
        // 未綁定/解除綁定，關黃燈
        yellow_led_off();
        g_yellow_led_on = false;
        break;

    case BLE_MESH_AT_EVENT_LINE_RECEIVED:
        // 藍燈短脈衝表示有收到行（非 VER 成功）
        pulse_blue(120);
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
}

// 數位 I/O 測試函數
void digital_io_test(void)
{
    // 讀取 PB.7 輸入狀態
    bool di_state = (PB7 != 0);

    // 根據 PB.7 輸入控制 PA.6 輸出
    // PB.7 = 1 時，PA.6 = 1
    // PB.7 = 0 時，PA.6 = 0
    PA6 = di_state ? 1 : 0;
}

// 設定 LED 顯示模式
void set_led_display_mode(led_display_mode_t mode)
{
    if (!g_led_control_enabled)
    {
        // LED 控制已停用
        return;
    }

    current_led_mode = mode;

    // 根據模式配置 LED 時序
    led_config_t led_configs[LED_COUNT];

    switch (mode)
    {
    case LED_MODE_NORMAL:
        // 正常模式：不同週期閃爍
        led_configs[0] = (led_config_t){PB, BIT1, 500, 100, false}; // 紅色
        led_configs[1] = (led_config_t){PB, BIT2, 300, 80, false};  // 黃色
        led_configs[2] = (led_config_t){PB, BIT3, 200, 60, false};  // 藍色
        break;

    case LED_MODE_SLOW:
        // 慢速模式：週期加倍
        led_configs[0] = (led_config_t){PB, BIT1, 1000, 200, false}; // 紅色
        led_configs[1] = (led_config_t){PB, BIT2, 600, 160, false};  // 黃色
        led_configs[2] = (led_config_t){PB, BIT3, 400, 120, false};  // 藍色
        break;

    case LED_MODE_FAST:
        // 快速模式：週期減半
        led_configs[0] = (led_config_t){PB, BIT1, 250, 50, false}; // 紅色
        led_configs[1] = (led_config_t){PB, BIT2, 150, 40, false}; // 黃色
        led_configs[2] = (led_config_t){PB, BIT3, 100, 30, false}; // 藍色
        break;

    case LED_MODE_WAVE:
        // 流水燈模式：依序點亮
        led_configs[0] = (led_config_t){PB, BIT1, 900, 300, false}; // 紅色
        led_configs[1] = (led_config_t){PB, BIT2, 900, 300, false}; // 黃色（延遲300ms）
        led_configs[2] = (led_config_t){PB, BIT3, 900, 300, false}; // 藍色（延遲600ms）
        break;

    default:
        return;
    }

    // 重新初始化 LED 控制器
    for (int i = 0; i < LED_COUNT; i++)
    {
        led_controller_init(&led_controllers[i], &led_configs[i]);
    }

    // 設定為閃爍模式
    led_manager_set_all_blinking(&led_manager);

    // 流水燈模式需要特殊處理延遲
    if (mode == LED_MODE_WAVE)
    {
        uint32_t current_time = get_system_time_ms();
        led_controller_set_state(&led_controllers[1], LED_STATE_BLINKING, current_time + 300);
        led_controller_set_state(&led_controllers[2], LED_STATE_BLINKING, current_time + 600);
    }
}

// 按鍵事件處理回調函數
void on_key_event(uint8_t key_index, key_event_t event, uint32_t press_duration)
{
    switch (key_index)
    {
    case KEY_A_INDEX:
        switch (event)
        {
        case KEY_EVENT_SHORT_PRESS:
            break;

        case KEY_EVENT_LONG_PRESS:
            // 長按5秒發送 AT+NR（自我解除綁定）
            if (press_duration >= 3000)
            {
                (void)ble_mesh_at_send_nr(&g_ble_at);
            }
            break;

        case KEY_EVENT_DOUBLE_CLICK:
            // 雙擊送 REBOOT（改由實際送指令）
            (void)ble_mesh_at_send_reboot(&g_ble_at);
            break;

        case KEY_EVENT_PRESS:
            break;

        case KEY_EVENT_RELEASE:
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
}

void led_system_init(void)
{
    // 若需要重新啟用 LED 管理，可將此函式內容恢復並設定 g_led_control_enabled=true
}

void key_system_init(void)
{
    // 按鍵 A 配置 (PB.15) - 按鍵連接到 PB.15
    key_config_t key_configs[KEY_COUNT] = {
        {PB, BIT15, true, 50, 3000, 1000} // PB.15, active_low, 50ms消抖, 3s長按, 1s雙擊間隔
    };

    // 初始化按鍵控制器
    for (int i = 0; i < KEY_COUNT; i++)
    {
        key_controller_init(&key_controllers[i], &key_configs[i]);
    }

    // 初始化按鍵管理器
    key_manager_init(&key_manager, key_controllers, KEY_COUNT, get_system_time_ms);
}

int main()
{
    SYS_Init();
    leds_basic_init();
    key_system_init();
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

    // LED 測試序列關閉；改由 UART1 AT 驗證結果控制黃燈

    while (1)
    {
        uint32_t current_time = g_systick_ms;

        // 更新所有按鍵 (非阻塞)
        key_manager_update_all(&key_manager, on_key_event);

        // 執行數位 I/O 測試
        digital_io_test();

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
            blue_led_off();
            g_blue_on_until_ms = 0;
        }

        // 持續事件處理與 LED 脈衝維持，不輸出文字
    }
}
