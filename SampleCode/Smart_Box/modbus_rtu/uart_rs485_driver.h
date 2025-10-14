#ifndef UART_RS485_DRIVER_H
#define UART_RS485_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "NuMicro.h"

typedef uint32_t (*uart_rs485_timestamp_cb_t)(void *context);
typedef void (*uart_rs485_rx_callback_t)(uint8_t byte, uint32_t timestamp_us, void *user_data);

typedef struct
{
    UART_T *uart;
    IRQn_Type irq_number;
    uint32_t module_clock;
    uint32_t baudrate;
    GPIO_T *dir_gpio_port;
    uint32_t dir_gpio_pin;
    uart_rs485_timestamp_cb_t timestamp_callback;
    void *timestamp_context;
} uart_rs485_driver_config_t;

typedef struct
{
    bool initialized;
    bool tx_active;
    uint32_t baudrate;
} uart_rs485_driver_state_t;

void uart_rs485_driver_init(const uart_rs485_driver_config_t *config);
void uart_rs485_driver_uninit(void);
bool uart_rs485_driver_set_baudrate(uint32_t baudrate);
uint32_t uart_rs485_driver_get_baudrate(void);
void uart_rs485_driver_set_rx_callback(uart_rs485_rx_callback_t callback, void *user_data);
bool uart_rs485_driver_write(const uint8_t *data, uint16_t length);
void uart_rs485_driver_flush(void);
void uart_rs485_driver_get_state(uart_rs485_driver_state_t *state);

void UART0_IRQHandler(void);

#endif
