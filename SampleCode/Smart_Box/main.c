#include <stdio.h>
#include <string.h>
#include "NuMicro.h"
#include "led_controller.h"
#include "key_controller.h"

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

// UART1（BLE AT）接收行緩衝
static volatile bool g_uart1_line_ready = false;
static char g_uart1_line[128];
static volatile uint32_t g_uart1_line_len = 0;
static volatile bool g_uart1_seen_cr = false;

// BLE AT 命令狀態
static volatile bool g_at_ver_sent = false;

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
    // UART1 時鐘
    CLK_EnableModuleClock(UART1_MODULE);
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_HXT, CLK_CLKDIV0_UART1(1));
    SystemCoreClockUpdate();

    // UART0 腳位設定
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

    // UART1 腳位設定（PA.8 = RXD, PA.9 = TXD）
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA8MFP_Msk | SYS_GPA_MFPH_PA9MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA8MFP_UART1_RXD | SYS_GPA_MFPH_PA9MFP_UART1_TXD);

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

// UART1（BLE AT）初始化與中斷啟用
static void UART1_BLE_Init(void)
{
    UART_Open(UART1, 115200);
    // 啟用接收資料可用與接收逾時中斷
    UART_ENABLE_INT(UART1, (UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk));
    NVIC_EnableIRQ(UART1_IRQn);
}

// UART1 傳送字串（阻塞式，簡易實作）
static void UART1_Send(const char *s)
{
    while (*s)
    {
        while (UART_GET_TX_FULL(UART1))
            ;
        UART_WRITE(UART1, (uint8_t)*s++);
    }
}

// UART1 處理單一接收位元組，偵測 CRLF 為一行結束
static inline void uart1_handle_rx_byte(uint8_t ch)
{
    if (g_uart1_line_ready)
    {
        // 尚未處理上一行，丟棄新資料以避免覆蓋
        return;
    }

    if (ch == '\r')
    {
        g_uart1_seen_cr = true;
        return;
    }

    if (ch == '\n')
    {
        if (g_uart1_seen_cr)
        {
            // 行結束
            g_uart1_line[g_uart1_line_len] = '\0';
            g_uart1_line_ready = true;
        }
        // 重置行狀態
        g_uart1_seen_cr = false;
        g_uart1_line_len = 0;
        return;
    }

    g_uart1_seen_cr = false;
    if (g_uart1_line_len < sizeof(g_uart1_line) - 1)
    {
        g_uart1_line[g_uart1_line_len++] = (char)ch;
    }
    else
    {
        // 超長，丟棄這行直到下一個 CRLF
        g_uart1_line_len = 0;
    }
}

void UART1_IRQHandler(void)
{
    uint32_t intsts = UART1->INTSTS;

    // 接收資料可用或接收逾時
    if (intsts & (UART_INTSTS_RDAIF_Msk | UART_INTSTS_RXTOIF_Msk))
    {
        while (!UART_GET_RX_EMPTY(UART1))
        {
            uint8_t ch = (uint8_t)UART_READ(UART1);
            uart1_handle_rx_byte(ch);
        }
    }

    // 清錯誤旗標（如有）
    uint32_t fifosts = UART1->FIFOSTS;
    if (fifosts & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART1->FIFOSTS = (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk);
    }
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

    printf("Digital I/O Initialized: PA.6 (DO), PB.7 (DI)\n");
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

    // 顯示 I/O 狀態（每 500ms 顯示一次）
    static uint32_t last_status_time = 0;
    uint32_t current_time = get_system_time_ms();

    if (current_time - last_status_time >= 500)
    {
        printf("Digital I/O Status - DI (PB.7): %s -> DO (PA.6): %s\n",
               di_state ? "HIGH" : "LOW",
               (PA6 != 0) ? "HIGH" : "LOW");

        last_status_time = current_time;
    }
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

    printf("LED Mode changed to: %d\n", mode);
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
            printf("Key A Short Press\n");
            break;

        case KEY_EVENT_LONG_PRESS:
            printf("Key A Long Press (%d ms)\n", press_duration);
            // 長按3秒發送 AT+VER 命令
            if (press_duration >= 3000)
            {
                if (!g_at_ver_sent)
                {
                    printf("Sending AT+VER command...\n");
                    UART1_Send("AT+VER\r\n");
                    g_at_ver_sent = true;
                    printf("AT+VER sent, waiting for response...\n");
                }
                else
                {
                    printf("AT+VER already sent\n");
                }
            }
            break;

        case KEY_EVENT_DOUBLE_CLICK:
            printf("Key A Double Click - Quick mode toggle\n");
            // 雙擊在快速和正常模式間切換（若 LED 控制已停用則忽略）
            if (g_led_control_enabled)
            {
                if (current_led_mode == LED_MODE_FAST)
                {
                    set_led_display_mode(LED_MODE_NORMAL);
                }
                else
                {
                    set_led_display_mode(LED_MODE_FAST);
                }
            }
            break;

        case KEY_EVENT_PRESS:
            printf("Key A Pressed\n");
            break;

        case KEY_EVENT_RELEASE:
            printf("Key A Released (held for %d ms)\n", press_duration);
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

    UART_Open(UART0, 115200);
    printf("%s: Smart Box with UART1 BLE AT, Key & Digital I/O\n", PROJECT_NAME);
    printf("Core Clock: %d Hz\n", SystemCoreClock);
    printf("LED control via manager: DISABLED (manual GPIO).\n");

    // UART1 (BLE AT) 初始化
    UART1_BLE_Init();
    printf("UART1 for BLE AT: 115200 8N1, TX=PA.9, RX=PA.8\n");
    // 先關閉黃燈，等待按鍵觸發 AT 命令後驗證通過再點亮
    yellow_led_off();
    printf("Press Key A for 3 seconds to send AT+VER\n");

    printf("\nKey Controls:\n");
    printf("- Short Press: Info message\n");
    printf("- Long Press (3s): Send AT+VER command\n");
    printf("- Double Click: (LED control disabled)\n");

    printf("\nDigital I/O Test:\n");
    printf("- PB.7 (DI): Input control signal\n");
    printf("- PA.6 (DO): Output follows input\n");
    printf("- Logic: PB.7=HIGH -> PA.6=HIGH, PB.7=LOW -> PA.6=LOW\n");

    // LED 測試序列關閉；改由 UART1 AT 驗證結果控制黃燈

    while (1)
    {
        uint32_t current_time = g_systick_ms;

        // 更新所有按鍵 (非阻塞)
        key_manager_update_all(&key_manager, on_key_event);

        // 執行數位 I/O 測試
        digital_io_test();

        // 若 UART1 接收到一行，進行解析
        if (g_uart1_line_ready)
        {
            // 只讀取一次指標，避免最佳化重排
            __DSB();
            printf("BLE AT Response: %s\r\n", g_uart1_line);

            if (strncmp(g_uart1_line, "VER-MSG SUCCESS", 15) == 0)
            {
                yellow_led_on();
                printf(" -> VER-MSG SUCCESS detected, Yellow LED ON\n");
            }
            else if (g_at_ver_sent)
            {
                printf(" -> Response received but not VER-MSG SUCCESS\n");
            }

            g_uart1_line_ready = false;
        }

        // 顯示當前按鍵狀態（僅在按下時）
        static bool last_key_state = false;
        bool current_key_state = key_manager_is_key_pressed(&key_manager, KEY_A_INDEX);
        if (current_key_state != last_key_state)
        {
            if (current_key_state)
            {
                printf("Key A is being pressed...\n");
            }
            last_key_state = current_key_state;
        }

        // 顯示長按進度（每500ms顯示一次）
        if (current_key_state)
        {
            static uint32_t last_progress_time = 0;
            if (current_time - last_progress_time >= 500)
            {
                uint32_t press_duration = key_manager_get_key_press_duration(&key_manager, KEY_A_INDEX);
                if (press_duration >= 1000)
                {
                    printf("Long press progress: %d ms\n", press_duration);
                }
                last_progress_time = current_time;
            }
        }

        // 每秒輸出一次時間資訊
        static uint32_t last_print_time = 0;
        if (current_time - last_print_time >= 1000)
        {
            printf("Runtime: %d.%03d s\n", current_time / 1000, current_time % 1000);
            last_print_time = current_time;
        }
    }
}
