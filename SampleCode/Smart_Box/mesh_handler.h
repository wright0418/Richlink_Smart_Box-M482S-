
#ifndef MESH_HANDLER_H
#define MESH_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_mesh_at.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Mesh 處理狀態結構
    typedef struct
    {
        volatile bool is_bound;
        char device_uid[32];
        char last_sender_uid[32];
        uint8_t last_payload[64];
        volatile uint32_t last_payload_len;
        volatile uint32_t msg_count;

        // Agent 請求緩衝
        volatile bool agent_request_pending;
        uint8_t pending_agent_data[64];
        volatile uint8_t pending_agent_length;
        volatile uint32_t agent_request_dropped;
    } mesh_handler_state_t;

    // 回調函數類型定義
    typedef void (*mesh_pa6_control_callback_t)(bool state);
    typedef void (*mesh_led_flash_callback_t)(uint32_t count);
    typedef void (*mesh_led_pulse_callback_t)(uint32_t duration_ms);
    typedef void (*mesh_led_binding_callback_t)(bool is_bound);
    typedef void (*mesh_led_yellow_callback_t)(bool on);
    typedef void (*mesh_agent_response_callback_t)(const uint8_t *data, uint8_t length);

    // Mesh 回調函數結構
    typedef struct
    {
        mesh_pa6_control_callback_t pa6_control;
        mesh_led_flash_callback_t led_flash;
        mesh_led_pulse_callback_t led_pulse_blue;
        mesh_led_pulse_callback_t led_pulse_red;
        mesh_led_binding_callback_t led_binding;
        mesh_led_yellow_callback_t led_yellow;
        mesh_agent_response_callback_t agent_response;
    } mesh_handler_callbacks_t;

    // Mesh 處理函數
    void mesh_handler_init(const mesh_handler_callbacks_t *callbacks);
    void mesh_handler_event(ble_mesh_at_event_t event, const char *data);

    // Mesh 行處理
    void mesh_handler_process_line(const char *line);

    // 獲取狀態
    const mesh_handler_state_t *mesh_handler_get_state(void);
    bool mesh_handler_is_bound(void);
    const char *mesh_handler_get_device_uid(void);

    // Agent 請求管理
    bool mesh_handler_has_pending_agent_request(void);
    bool mesh_handler_get_pending_agent_request(uint8_t *data, uint8_t *length);

#ifdef __cplusplus
}
#endif

#endif // MESH_HANDLER_H