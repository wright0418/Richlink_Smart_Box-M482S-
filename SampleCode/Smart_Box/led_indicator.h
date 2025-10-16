#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// LED 控制常數
#define YELLOW_FLASH_ON_MS 100u
#define YELLOW_FLASH_OFF_MS 150u
#define BLUE_HEARTBEAT_PERIOD_MS 2000u
#define BLUE_HEARTBEAT_ON_MS 200u

    // LED 狀態結構
    typedef struct
    {
        volatile uint32_t red_on_until_ms;
        volatile uint32_t blue_on_until_ms;
        volatile uint32_t yellow_flash_count;
        volatile uint32_t yellow_flash_next_ms;
        volatile bool yellow_flash_on;
        volatile uint32_t blue_heartbeat_last_ms;
        volatile bool provisioning_wait;
        volatile bool is_bound;
        volatile bool yellow_led_on;
    } led_indicator_state_t;

    // LED 控制函數
    void led_indicator_init(void);
    void led_indicator_update(uint32_t current_time);

    // 基礎 LED 控制
    void led_red_on(void);
    void led_red_off(void);
    void led_blue_on(void);
    void led_blue_off(void);
    void led_yellow_on(void);
    void led_yellow_off(void);

    // LED 脈衝和閃爍控制
    void led_pulse_red(uint32_t duration_ms);
    void led_pulse_blue(uint32_t duration_ms);
    void led_flash_yellow(uint32_t count);

    // LED 狀態管理
    void led_set_binding_state(bool is_bound);
    void led_set_provisioning_wait(bool wait);
    void led_set_yellow_status(bool on);

    // 獲取 LED 狀態
    const led_indicator_state_t *led_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // LED_INDICATOR_H