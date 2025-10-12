#ifndef KEY_CONTROLLER_H
#define KEY_CONTROLLER_H

#include "NuMicro.h"
#include <stdint.h>
#include <stdbool.h>

// 按鍵事件類型
typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,       // 按下
    KEY_EVENT_RELEASE,     // 釋放
    KEY_EVENT_SHORT_PRESS, // 短按 (釋放時觸發)
    KEY_EVENT_LONG_PRESS,  // 長按 (持續按住達到時間)
    KEY_EVENT_DOUBLE_CLICK // 雙擊
} key_event_t;

// 按鍵狀態
typedef enum
{
    KEY_STATE_IDLE = 0,
    KEY_STATE_DEBOUNCE,   // 消抖中
    KEY_STATE_PRESSED,    // 已按下
    KEY_STATE_WAIT_DOUBLE // 等待雙擊
} key_state_t;

// 按鍵配置結構
typedef struct
{
    GPIO_T *port;                      // GPIO 埠
    uint32_t pin_mask;                 // 腳位遮罩
    bool active_low;                   // true: 按下為低電位, false: 按下為高電位
    uint32_t debounce_ms;              // 消抖時間 (毫秒)
    uint32_t long_press_ms;            // 長按時間 (毫秒)
    uint32_t double_click_interval_ms; // 雙擊間隔時間 (毫秒)
} key_config_t;

// 按鍵控制器結構
typedef struct
{
    key_config_t config;        // 按鍵配置
    key_state_t state;          // 當前狀態
    bool current_level;         // 當前電位狀態
    bool last_stable_level;     // 上次穩定電位狀態
    uint32_t state_start_time;  // 狀態開始時間
    uint32_t press_start_time;  // 按下開始時間
    uint32_t last_release_time; // 上次釋放時間
    bool long_press_triggered;  // 長按已觸發標誌
    uint8_t click_count;        // 點擊計數
} key_controller_t;

// 按鍵管理器結構
typedef struct
{
    key_controller_t *controllers; // 按鍵控制器陣列
    uint8_t count;                 // 按鍵數量
    uint32_t (*get_time_ms)(void); // 取得時間函數指標
} key_manager_t;

// 按鍵事件回調函數類型
typedef void (*key_event_callback_t)(uint8_t key_index, key_event_t event, uint32_t press_duration);

// 函數宣告
void key_controller_init(key_controller_t *controller, const key_config_t *config);
key_event_t key_controller_update(key_controller_t *controller, uint32_t current_time);
bool key_controller_is_pressed(key_controller_t *controller);
uint32_t key_controller_get_press_duration(key_controller_t *controller, uint32_t current_time);

void key_manager_init(key_manager_t *manager, key_controller_t *controllers, uint8_t count, uint32_t (*get_time_func)(void));
void key_manager_update_all(key_manager_t *manager, key_event_callback_t callback);
bool key_manager_is_key_pressed(key_manager_t *manager, uint8_t key_index);
uint32_t key_manager_get_key_press_duration(key_manager_t *manager, uint8_t key_index);

#endif // KEY_CONTROLLER_H