#ifndef DIGITAL_IO_H
#define DIGITAL_IO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 按鍵去抖常數
#define KEY_DEBOUNCE_MS 30u
#define KEY_A_LONG_PRESS_MS 5000u

// PA6 自動關閉常數
#define PA6_ON_HOLD_MS 5000u

// 數位 I/O 狀態結構
typedef struct {
    // KeyA 按鍵狀態 (PB.15 - active low)
    volatile bool key_a_pressed;
    volatile uint32_t key_a_press_start_ms;
    volatile bool key_a_long_press_sent;
    volatile uint32_t key_a_last_change_ms;
    
    // PA.6 由 Mesh 指令控制的自動關閉計時
    volatile uint32_t pa6_auto_off_deadline_ms;
} digital_io_state_t;

// 回調函數類型定義
typedef void (*key_a_long_press_callback_t)(void);

// 數位 I/O 控制函數
void digital_io_init(void);
void digital_io_update(uint32_t current_time);

// 按鍵處理
void digital_io_set_key_callback(key_a_long_press_callback_t callback);

// PA6 控制（由 Mesh 指令控制）
void digital_io_set_pa6(bool state);
void digital_io_set_pa6_with_auto_off(bool state);

// 數位 I/O 測試函數
void digital_io_test(void);

// 獲取狀態
const digital_io_state_t *digital_io_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // DIGITAL_IO_H