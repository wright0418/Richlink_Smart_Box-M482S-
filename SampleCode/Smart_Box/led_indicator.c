#include "led_indicator.h"
#include "../../../Library/Device/Nuvoton/M480/Include/NuMicro.h"

// 外部系統時間獲取函數
extern volatile uint32_t g_systick_ms;

// LED 狀態全域變數
static led_indicator_state_t g_led_state = {0};

void led_indicator_init(void)
{
    // LED 腳位基礎初始化（不使用 LED 控制器，直接作為 GPIO 輸出）
    GPIO_SetMode(PB, BIT1 | BIT2 | BIT3, GPIO_MODE_OUTPUT);
    // 預設全滅（LED 為 active-high）
    PB1 = 0;
    PB2 = 0;
    PB3 = 0;

    // 初始化狀態
    g_led_state.red_on_until_ms = 0;
    g_led_state.blue_on_until_ms = 0;
    g_led_state.red_flash_count = 0;
    g_led_state.red_flash_next_ms = 0;
    g_led_state.red_flash_on = false;
    g_led_state.yellow_flash_count = 0;
    g_led_state.yellow_flash_next_ms = 0;
    g_led_state.yellow_flash_on = false;
    g_led_state.blue_heartbeat_last_ms = g_systick_ms;
    g_led_state.provisioning_wait = false;
    g_led_state.is_bound = false;
    g_led_state.yellow_led_on = false;
}

void led_indicator_update(uint32_t current_time)
{
    // 無阻塞 LED 脈衝維持：逾時到期自動熄滅（使用差值比較避免溢位）
    if (g_led_state.red_on_until_ms && (int32_t)(current_time - g_led_state.red_on_until_ms) >= 0)
    {
        led_red_off();
        g_led_state.red_on_until_ms = 0;
    }

    if (g_led_state.blue_on_until_ms && (int32_t)(current_time - g_led_state.blue_on_until_ms) >= 0)
    {
        // 如果已綁定，保持藍燈常亮；否則依脈衝到期時間關閉
        if (!g_led_state.is_bound)
        {
            led_blue_off();
        }
        g_led_state.blue_on_until_ms = 0;
    }

    // 紅燈快閃狀態機（Modbus 偵測結果指示）
    if (g_led_state.red_flash_count > 0 && (int32_t)(current_time - g_led_state.red_flash_next_ms) >= 0)
    {
        if (!g_led_state.red_flash_on)
        {
            // 開始一次閃爍
            led_red_on();
            g_led_state.red_flash_on = true;
            g_led_state.red_flash_next_ms = current_time + RED_FLASH_ON_MS;
        }
        else
        {
            // 結束一次閃爍
            led_red_off();
            g_led_state.red_flash_on = false;
            g_led_state.red_flash_count--;
            if (g_led_state.red_flash_count > 0)
            {
                g_led_state.red_flash_next_ms = current_time + RED_FLASH_OFF_MS;
            }
        }
    }

    // 黃燈快閃狀態機（MDTSG/MDTPG 指示）
    if (g_led_state.yellow_flash_count > 0 && (int32_t)(current_time - g_led_state.yellow_flash_next_ms) >= 0)
    {
        if (!g_led_state.yellow_flash_on)
        {
            // 開始一次閃爍
            led_yellow_on();
            g_led_state.yellow_flash_on = true;
            g_led_state.yellow_flash_next_ms = current_time + YELLOW_FLASH_ON_MS;
        }
        else
        {
            // 結束一次閃爍
            led_yellow_off();
            g_led_state.yellow_flash_on = false;
            g_led_state.yellow_flash_count--;
            if (g_led_state.yellow_flash_count > 0)
            {
                g_led_state.yellow_flash_next_ms = current_time + YELLOW_FLASH_OFF_MS;
            }
        }
    }

    // 未綁定時的藍燈心跳（每 2 秒閃 0.2 秒）
    if (g_led_state.provisioning_wait && !g_led_state.is_bound)
    {
        if ((int32_t)(current_time - g_led_state.blue_heartbeat_last_ms - BLUE_HEARTBEAT_PERIOD_MS) >= 0)
        {
            led_pulse_blue(BLUE_HEARTBEAT_ON_MS);
            g_led_state.blue_heartbeat_last_ms = current_time;
        }
    }
}

// 基礎 LED 控制
void led_red_on(void)
{
    PB1 = 1;
}

void led_red_off(void)
{
    PB1 = 0;
}

void led_blue_on(void)
{
    PB3 = 1;
}

void led_blue_off(void)
{
    PB3 = 0;
}

void led_yellow_on(void)
{
    PB2 = 1;
}

void led_yellow_off(void)
{
    PB2 = 0;
}

// LED 脈衝控制
void led_pulse_red(uint32_t duration_ms)
{
    led_red_on();
    g_led_state.red_on_until_ms = g_systick_ms + duration_ms;
}

void led_pulse_blue(uint32_t duration_ms)
{
    led_blue_on();
    g_led_state.blue_on_until_ms = g_systick_ms + duration_ms;
}

// 黃燈快閃 N 次（例如 MDTSG=1, MDTPG=2）
// 支援累加模式，避免事件遺漏
void led_flash_yellow(uint32_t count)
{
    if (g_led_state.yellow_flash_count == 0)
    {
        // 新開始閃爍
        g_led_state.yellow_flash_count = count;
        g_led_state.yellow_flash_next_ms = g_systick_ms + 10; // 立即開始
        g_led_state.yellow_flash_on = false;
    }
    else
    {
        // 累加閃爍次數，但限制最大值避免無限排隊
        g_led_state.yellow_flash_count += count;
        if (g_led_state.yellow_flash_count > 10)
        {
            g_led_state.yellow_flash_count = 10; // 最多排隊10次
        }
    }
}

// 紅燈快閃 N 次（Modbus 偵測結果指示）
void led_flash_red(uint32_t count)
{
    if (g_led_state.red_flash_count == 0)
    {
        // 新開始閃爍
        g_led_state.red_flash_count = count;
        g_led_state.red_flash_next_ms = g_systick_ms + 10; // 立即開始
        g_led_state.red_flash_on = false;
    }
    else
    {
        // 累加閃爍次數，但限制最大值避免無限排隊
        g_led_state.red_flash_count += count;
        if (g_led_state.red_flash_count > 10)
        {
            g_led_state.red_flash_count = 10; // 最多排隊10次
        }
    }
}

// LED 狀態管理
void led_set_binding_state(bool is_bound)
{
    g_led_state.is_bound = is_bound;
    if (is_bound)
    {
        led_blue_on();
        g_led_state.provisioning_wait = false;
    }
    else
    {
        led_blue_off();
        g_led_state.provisioning_wait = true;
    }
}

void led_set_provisioning_wait(bool wait)
{
    g_led_state.provisioning_wait = wait;
    if (wait)
    {
        g_led_state.blue_heartbeat_last_ms = g_systick_ms;
    }
}

void led_set_yellow_status(bool on)
{
    g_led_state.yellow_led_on = on;
    if (on)
    {
        led_yellow_on();
    }
    else
    {
        led_yellow_off();
    }
}

// 獲取 LED 狀態
const led_indicator_state_t *led_get_state(void)
{
    return &g_led_state;
}