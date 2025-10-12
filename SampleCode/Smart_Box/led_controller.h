#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "NuMicro.h"
#include <stdint.h>
#include <stdbool.h>

// LED 狀態定義
typedef enum
{
    LED_STATE_OFF = 0,
    LED_STATE_ON = 1,
    LED_STATE_BLINKING = 2
} led_state_t;

// LED 配置結構
typedef struct
{
    GPIO_T *port;        // GPIO 埠
    uint32_t pin_mask;   // 腳位遮罩
    uint32_t period_ms;  // 閃爍週期 (毫秒)
    uint32_t on_time_ms; // 點亮時間 (毫秒)
    bool active_low;     // true: 低電位點亮, false: 高電位點亮
} led_config_t;

// LED 控制器結構
typedef struct
{
    led_config_t config;    // LED 配置
    led_state_t state;      // 當前狀態
    uint32_t start_time_ms; // 開始時間
    bool current_output;    // 當前輸出狀態
} led_controller_t;

// LED 控制器管理結構
typedef struct
{
    led_controller_t *controllers; // LED 控制器陣列
    uint8_t count;                 // LED 數量
    uint32_t (*get_time_ms)(void); // 取得時間函數指標
} led_manager_t;

// 函數宣告
void led_controller_init(led_controller_t *controller, const led_config_t *config);
void led_controller_set_state(led_controller_t *controller, led_state_t state, uint32_t current_time_ms);
void led_controller_update(led_controller_t *controller, uint32_t current_time_ms);

void led_manager_init(led_manager_t *manager, led_controller_t *controllers, uint8_t count, uint32_t (*get_time_func)(void));
void led_manager_set_led_state(led_manager_t *manager, uint8_t led_index, led_state_t state);
void led_manager_update_all(led_manager_t *manager);

// 便利函數
void led_manager_set_all_off(led_manager_t *manager);
void led_manager_set_all_on(led_manager_t *manager);
void led_manager_set_all_blinking(led_manager_t *manager);

#endif // LED_CONTROLLER_H