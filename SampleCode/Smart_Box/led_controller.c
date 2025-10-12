#include "led_controller.h"

// LED 控制器初始化
void led_controller_init(led_controller_t *controller, const led_config_t *config)
{
    if (!controller || !config)
        return;

    controller->config = *config;
    controller->state = LED_STATE_OFF;
    controller->start_time_ms = 0;
    controller->current_output = false;

    // 設定 GPIO 為輸出模式
    GPIO_SetMode(config->port, config->pin_mask, GPIO_MODE_OUTPUT);

    // 初始化為關閉狀態
    if (config->active_low)
    {
        config->port->DOUT |= config->pin_mask; // 高電位關閉
    }
    else
    {
        config->port->DOUT &= ~config->pin_mask; // 低電位關閉
    }
}

// 設定 LED 狀態
void led_controller_set_state(led_controller_t *controller, led_state_t state, uint32_t current_time_ms)
{
    if (!controller)
        return;

    controller->state = state;
    controller->start_time_ms = current_time_ms;

    switch (state)
    {
    case LED_STATE_OFF:
        controller->current_output = false;
        if (controller->config.active_low)
        {
            controller->config.port->DOUT |= controller->config.pin_mask;
        }
        else
        {
            controller->config.port->DOUT &= ~controller->config.pin_mask;
        }
        break;

    case LED_STATE_ON:
        controller->current_output = true;
        if (controller->config.active_low)
        {
            controller->config.port->DOUT &= ~controller->config.pin_mask;
        }
        else
        {
            controller->config.port->DOUT |= controller->config.pin_mask;
        }
        break;

    case LED_STATE_BLINKING:
        // 在 update 函數中處理閃爍邏輯
        break;
    }
}

// 更新 LED 狀態
void led_controller_update(led_controller_t *controller, uint32_t current_time_ms)
{
    if (!controller || controller->state != LED_STATE_BLINKING)
        return;

    uint32_t elapsed_time = current_time_ms - controller->start_time_ms;
    uint32_t phase = elapsed_time % controller->config.period_ms;
    bool should_be_on = (phase < controller->config.on_time_ms);

    if (should_be_on != controller->current_output)
    {
        controller->current_output = should_be_on;

        if (should_be_on)
        {
            // 點亮 LED
            if (controller->config.active_low)
            {
                controller->config.port->DOUT &= ~controller->config.pin_mask;
            }
            else
            {
                controller->config.port->DOUT |= controller->config.pin_mask;
            }
        }
        else
        {
            // 熄滅 LED
            if (controller->config.active_low)
            {
                controller->config.port->DOUT |= controller->config.pin_mask;
            }
            else
            {
                controller->config.port->DOUT &= ~controller->config.pin_mask;
            }
        }
    }
}

// LED 管理器初始化
void led_manager_init(led_manager_t *manager, led_controller_t *controllers, uint8_t count, uint32_t (*get_time_func)(void))
{
    if (!manager || !controllers || !get_time_func)
        return;

    manager->controllers = controllers;
    manager->count = count;
    manager->get_time_ms = get_time_func;
}

// 設定特定 LED 狀態
void led_manager_set_led_state(led_manager_t *manager, uint8_t led_index, led_state_t state)
{
    if (!manager || led_index >= manager->count)
        return;

    uint32_t current_time = manager->get_time_ms();
    led_controller_set_state(&manager->controllers[led_index], state, current_time);
}

// 更新所有 LED
void led_manager_update_all(led_manager_t *manager)
{
    if (!manager)
        return;

    uint32_t current_time = manager->get_time_ms();

    for (uint8_t i = 0; i < manager->count; i++)
    {
        led_controller_update(&manager->controllers[i], current_time);
    }
}

// 便利函數：關閉所有 LED
void led_manager_set_all_off(led_manager_t *manager)
{
    if (!manager)
        return;

    for (uint8_t i = 0; i < manager->count; i++)
    {
        led_manager_set_led_state(manager, i, LED_STATE_OFF);
    }
}

// 便利函數：點亮所有 LED
void led_manager_set_all_on(led_manager_t *manager)
{
    if (!manager)
        return;

    for (uint8_t i = 0; i < manager->count; i++)
    {
        led_manager_set_led_state(manager, i, LED_STATE_ON);
    }
}

// 便利函數：所有 LED 閃爍
void led_manager_set_all_blinking(led_manager_t *manager)
{
    if (!manager)
        return;

    for (uint8_t i = 0; i < manager->count; i++)
    {
        led_manager_set_led_state(manager, i, LED_STATE_BLINKING);
    }
}