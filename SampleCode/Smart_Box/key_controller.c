#include "key_controller.h"

// 讀取按鍵當前電位
static bool read_key_level(const key_config_t *config)
{
    bool pin_level = (config->port->PIN & config->pin_mask) != 0;

    // 根據 active_low 配置返回邏輯電位
    if (config->active_low)
    {
        return !pin_level; // 低電位為按下
    }
    else
    {
        return pin_level; // 高電位為按下
    }
}

// 按鍵控制器初始化
void key_controller_init(key_controller_t *controller, const key_config_t *config)
{
    if (!controller || !config)
        return;

    controller->config = *config;
    controller->state = KEY_STATE_IDLE;
    controller->current_level = false;
    controller->last_stable_level = false;
    controller->state_start_time = 0;
    controller->press_start_time = 0;
    controller->last_release_time = 0;
    controller->long_press_triggered = false;
    controller->click_count = 0;

    // 設定 GPIO 為輸入模式
    GPIO_SetMode(config->port, config->pin_mask, GPIO_MODE_INPUT);

    // 如果是 active_low，啟用內部上拉電阻
    if (config->active_low)
    {
        GPIO_SetPullCtl(config->port, config->pin_mask, GPIO_PUSEL_PULL_UP);
    }
    else
    {
        GPIO_SetPullCtl(config->port, config->pin_mask, GPIO_PUSEL_PULL_DOWN);
    }

    // 讀取初始狀態
    controller->current_level = read_key_level(config);
    controller->last_stable_level = controller->current_level;
}

// 按鍵控制器更新
key_event_t key_controller_update(key_controller_t *controller, uint32_t current_time)
{
    if (!controller)
        return KEY_EVENT_NONE;

    bool current_level = read_key_level(&controller->config);
    key_event_t event = KEY_EVENT_NONE;

    switch (controller->state)
    {
    case KEY_STATE_IDLE:
        if (current_level != controller->last_stable_level)
        {
            // 電位變化，開始消抖
            controller->state = KEY_STATE_DEBOUNCE;
            controller->state_start_time = current_time;
            controller->current_level = current_level;
        }

        // 檢查雙擊超時
        if (controller->click_count > 0 &&
            (current_time - controller->last_release_time) > controller->config.double_click_interval_ms)
        {
            controller->click_count = 0;
        }
        break;

    case KEY_STATE_DEBOUNCE:
        if (current_level != controller->current_level)
        {
            // 電位再次變化，重新開始消抖
            controller->current_level = current_level;
            controller->state_start_time = current_time;
        }
        else if ((current_time - controller->state_start_time) >= controller->config.debounce_ms)
        {
            // 消抖完成，確認狀態
            if (controller->current_level && !controller->last_stable_level)
            {
                // 按下確認
                controller->state = KEY_STATE_PRESSED;
                controller->press_start_time = current_time;
                controller->long_press_triggered = false;
                controller->last_stable_level = true;
                event = KEY_EVENT_PRESS;
            }
            else if (!controller->current_level && controller->last_stable_level)
            {
                // 釋放確認
                controller->state = KEY_STATE_WAIT_DOUBLE;
                controller->last_release_time = current_time;
                controller->last_stable_level = false;
                controller->click_count++;
                event = KEY_EVENT_RELEASE;
            }
            else
            {
                // 回到空閒狀態
                controller->state = KEY_STATE_IDLE;
            }
        }
        break;

    case KEY_STATE_PRESSED:
        if (current_level != controller->last_stable_level)
        {
            // 按鍵釋放
            controller->state = KEY_STATE_DEBOUNCE;
            controller->state_start_time = current_time;
            controller->current_level = current_level;
        }
        else if (!controller->long_press_triggered &&
                 (current_time - controller->press_start_time) >= controller->config.long_press_ms)
        {
            // 長按觸發
            controller->long_press_triggered = true;
            event = KEY_EVENT_LONG_PRESS;
        }
        break;

    case KEY_STATE_WAIT_DOUBLE:
        if (current_level && !controller->last_stable_level)
        {
            // 第二次按下
            controller->state = KEY_STATE_DEBOUNCE;
            controller->state_start_time = current_time;
            controller->current_level = current_level;
        }
        else if ((current_time - controller->last_release_time) > controller->config.double_click_interval_ms)
        {
            // 雙擊超時，判定為單擊
            if (controller->click_count == 1)
            {
                event = KEY_EVENT_SHORT_PRESS;
            }
            else if (controller->click_count >= 2)
            {
                event = KEY_EVENT_DOUBLE_CLICK;
            }
            controller->click_count = 0;
            controller->state = KEY_STATE_IDLE;
        }
        break;
    }

    return event;
}

// 檢查按鍵是否正在被按下
bool key_controller_is_pressed(key_controller_t *controller)
{
    if (!controller)
        return false;
    return controller->last_stable_level;
}

// 取得按下持續時間
uint32_t key_controller_get_press_duration(key_controller_t *controller, uint32_t current_time)
{
    if (!controller || !controller->last_stable_level)
        return 0;
    return current_time - controller->press_start_time;
}

// 按鍵管理器初始化
void key_manager_init(key_manager_t *manager, key_controller_t *controllers, uint8_t count, uint32_t (*get_time_func)(void))
{
    if (!manager || !controllers || !get_time_func)
        return;

    manager->controllers = controllers;
    manager->count = count;
    manager->get_time_ms = get_time_func;
}

// 更新所有按鍵
void key_manager_update_all(key_manager_t *manager, key_event_callback_t callback)
{
    if (!manager)
        return;

    uint32_t current_time = manager->get_time_ms();

    for (uint8_t i = 0; i < manager->count; i++)
    {
        key_event_t event = key_controller_update(&manager->controllers[i], current_time);

        if (event != KEY_EVENT_NONE && callback)
        {
            uint32_t duration = 0;
            if (event == KEY_EVENT_LONG_PRESS || event == KEY_EVENT_RELEASE)
            {
                duration = key_controller_get_press_duration(&manager->controllers[i], current_time);
            }
            callback(i, event, duration);
        }
    }
}

// 檢查特定按鍵是否被按下
bool key_manager_is_key_pressed(key_manager_t *manager, uint8_t key_index)
{
    if (!manager || key_index >= manager->count)
        return false;
    return key_controller_is_pressed(&manager->controllers[key_index]);
}

// 取得特定按鍵的按下時間
uint32_t key_manager_get_key_press_duration(key_manager_t *manager, uint8_t key_index)
{
    if (!manager || key_index >= manager->count)
        return 0;
    uint32_t current_time = manager->get_time_ms();
    return key_controller_get_press_duration(&manager->controllers[key_index], current_time);
}