#include "../../../Library/Device/Nuvoton/M480/Include/NuMicro.h" // IWYU pragma: keep
#include "digital_io.h"

// 定義 NULL 為 (void*)0 以支援 C99（若尚未定義）
#ifndef NULL
#define NULL ((void *)0)
#endif

// 外部系統時間獲取函數
extern volatile uint32_t g_systick_ms;

// 數位 I/O 狀態全域變數
static digital_io_state_t g_digital_io_state = {0};
static key_a_long_press_callback_t g_key_a_callback = (void *)0;
static di_change_callback_t g_di_callback = (void *)0;
static bool g_di_state_cached = false; // 真正的 DI 狀態（語意：1 表示有效，active-low PB7==0）
static uint32_t g_di_last_change_ms = 0;
static const uint32_t DI_DEBOUNCE_MS = 30u;

// 透過一個極小的內聯函式引用 GPIO 名稱，避免靜態分析器誤判未使用 NuMicro.h
static inline void __nu_dummy_use(void)
{
    (void)PA6;
    (void)PB7;
    (void)PB15;
}

void digital_io_init(void)
{
    // PA.6 設定為輸出 (DO)
    GPIO_SetMode(PA, BIT6, GPIO_MODE_OUTPUT);
    PA6 = 0; // 初始輸出低電位

    // PB.7 設定為輸入 (DI)
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    // GPIO_SetPullCtl(PB, BIT7, GPIO_PUSEL_PULL_DOWN); // 設定下拉電阻

    // 初始化 DI 緩存狀態（active-low 轉正邏輯）
    g_di_state_cached = (PB7 == 0);
    g_di_last_change_ms = g_systick_ms;

#ifdef MODBUS_RTU
    // PB.14 作為 RS485 DIR 控制腳
    GPIO_SetMode(PB, BIT14, GPIO_MODE_OUTPUT);
    PB14 = 0; // 預設接收模式
#endif

    // KeyA (PB.15) 設定為輸入 (active low)
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
    GPIO_SetPullCtl(PB, BIT15, GPIO_PUSEL_PULL_UP); // 設定上拉電阻

    // 初始化狀態
    g_digital_io_state.key_a_pressed = false;
    g_digital_io_state.key_a_press_start_ms = 0;
    g_digital_io_state.key_a_long_press_sent = false;
    g_digital_io_state.key_a_last_change_ms = 0;
    g_digital_io_state.pa6_auto_off_deadline_ms = 0;
}

void digital_io_update(uint32_t current_time)
{
    // KeyA 按鍵處理函數 (加入去抖功能)
    bool key_pressed = (PB15 == 0); // active low

    // 檢查按鍵狀態是否改變
    bool state_changed = (key_pressed != g_digital_io_state.key_a_pressed);

    if (state_changed)
    {
        // 狀態改變，檢查去抖時間
        if ((int32_t)(current_time - g_digital_io_state.key_a_last_change_ms - KEY_DEBOUNCE_MS) >= 0)
        {
            // 去抖時間已過，接受狀態改變
            g_digital_io_state.key_a_last_change_ms = current_time;

            if (key_pressed && !g_digital_io_state.key_a_pressed)
            {
                // 按鍵剛被按下
                g_digital_io_state.key_a_pressed = true;
                g_digital_io_state.key_a_press_start_ms = current_time;
                g_digital_io_state.key_a_long_press_sent = false;
            }
            else if (!key_pressed && g_digital_io_state.key_a_pressed)
            {
                // 按鍵剛被釋放
                g_digital_io_state.key_a_pressed = false;
                g_digital_io_state.key_a_long_press_sent = false;
            }
        }
        // 否則忽略此次狀態改變（在去抖期間內）
    }
    else if (key_pressed && g_digital_io_state.key_a_pressed && !g_digital_io_state.key_a_long_press_sent)
    {
        // 按鍵持續按下，檢查是否達到長按時間
        if ((int32_t)(current_time - g_digital_io_state.key_a_press_start_ms - KEY_A_LONG_PRESS_MS) >= 0)
        {
            // 長按 5 秒，調用回調函數
            if (g_key_a_callback != (void *)0)
            {
                g_key_a_callback();
            }
            g_digital_io_state.key_a_long_press_sent = true; // 防止重複發送
        }
    }

    // PA6 Mesh ON 保持計時：逾時則自動關閉
    if (g_digital_io_state.pa6_auto_off_deadline_ms &&
        (int32_t)(current_time - g_digital_io_state.pa6_auto_off_deadline_ms) >= 0)
    {
        PA6 = 0; // 自動 OFF
        g_digital_io_state.pa6_auto_off_deadline_ms = 0;
    }

    // DI 變化偵測（active-low PB7）
    bool di_now = (PB7 == 0);
    if (di_now != g_di_state_cached)
    {
        // 去抖
        if ((int32_t)(current_time - g_di_last_change_ms - DI_DEBOUNCE_MS) >= 0)
        {
            g_di_state_cached = di_now;
            g_di_last_change_ms = current_time;
            if (g_di_callback != (void *)0)
            {
                g_di_callback(g_di_state_cached);
            }
        }
    }
}

void digital_io_set_key_callback(key_a_long_press_callback_t callback)
{
    g_key_a_callback = callback;
}

void digital_io_set_di_callback(di_change_callback_t callback)
{
    g_di_callback = callback;
}

void digital_io_set_pa6(bool state)
{
    PA6 = state ? 1 : 0;
    if (!state)
    {
        g_digital_io_state.pa6_auto_off_deadline_ms = 0; // 清除自動關閉計時
    }
}

void digital_io_set_pa6_with_auto_off(bool state)
{
    if (state)
    {
        // ON 命令：開啟 PA6 並起算/延長 5 秒自動關閉
        PA6 = 1;
        g_digital_io_state.pa6_auto_off_deadline_ms = g_systick_ms + PA6_ON_HOLD_MS;
    }
    else
    {
        // OFF 命令：立即關閉 PA6 並清除計時
        PA6 = 0;
        g_digital_io_state.pa6_auto_off_deadline_ms = 0;
    }
}

// 數位 I/O 測試函數
void digital_io_test(void)
{
    // 需求更新：不再用 PB.7 控制 PA.6，保留為空實作
}

const digital_io_state_t *digital_io_get_state(void)
{
    return &g_digital_io_state;
}

bool digital_io_get_di(void)
{
    // PB.7 設為輸入，直接讀取腳位邏輯值
    // 該 DI 為 active-low（參考 digital_io_test）：PB7==0 視為 DI=1
    return (PB7 == 0);
}

bool digital_io_get_do(void)
{
    // PA.6 為輸出腳，直接讀回輸出暫存器狀態
    return (PA6 != 0);
}