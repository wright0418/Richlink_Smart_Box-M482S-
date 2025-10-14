#include "uart_rs485_driver.h"

#include <stddef.h>

typedef struct
{
    uart_rs485_driver_config_t config;
    uart_rs485_rx_callback_t rx_callback;
    void *rx_user_data;
    bool initialized;
    bool tx_active;
    uint32_t baudrate;
} uart_rs485_driver_context_t;

static uart_rs485_driver_context_t g_uart_rs485 = {0};

static bool uart_rs485_is_locked(void)
{
    return (SYS->REGLCTL == 0U);
}

static void uart_rs485_set_dir_tx(void)
{
    if (g_uart_rs485.config.dir_gpio_port != NULL)
    {
        g_uart_rs485.config.dir_gpio_port->DOUT |= (uint32_t)(1UL << g_uart_rs485.config.dir_gpio_pin);
    }
}

static void uart_rs485_set_dir_rx(void)
{
    if (g_uart_rs485.config.dir_gpio_port != NULL)
    {
        g_uart_rs485.config.dir_gpio_port->DOUT &= ~(uint32_t)(1UL << g_uart_rs485.config.dir_gpio_pin);
    }
}

static uint32_t uart_rs485_get_timestamp_us(void)
{
    if (g_uart_rs485.config.timestamp_callback != NULL)
    {
        return g_uart_rs485.config.timestamp_callback(g_uart_rs485.config.timestamp_context);
    }
    return 0U;
}

static void uart_rs485_clear_fifo_errors(UART_T *uart)
{
    uint32_t fifo = uart->FIFOSTS;
    if ((fifo & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk)) != 0U)
    {
        uart->FIFOSTS = UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk;
    }
}

static void uart_rs485_enable_clock(uint32_t module_clock)
{
    bool was_locked = uart_rs485_is_locked();
    if (was_locked)
    {
        SYS_UnlockReg();
    }

    CLK_EnableModuleClock(module_clock);

    if (was_locked)
    {
        SYS_LockReg();
    }
}

static void uart_rs485_configure_uart(UART_T *uart, uint32_t baudrate)
{
    UART_Open(uart, baudrate);
    UART_EnableInt(uart, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    UART_SetTimeoutCnt(uart, 0x40U);
}

static void uart_rs485_disable_uart(void)
{
    UART_T *uart = g_uart_rs485.config.uart;
    if (uart == NULL)
    {
        return;
    }

    uart->INTEN = 0U;
    UART_Close(uart);
}

static void uart_rs485_handle_rx_irq(UART_T *uart)
{
    while (UART_GET_RX_EMPTY(uart) == 0U)
    {
        uint8_t data = (uint8_t)UART_READ(uart);
        uint32_t timestamp = uart_rs485_get_timestamp_us();
        if (g_uart_rs485.rx_callback != NULL)
        {
            g_uart_rs485.rx_callback(data, timestamp, g_uart_rs485.rx_user_data);
        }
    }
}

static void uart_rs485_driver_irq_handler(UART_T *uart)
{
    uint32_t intsts = uart->INTSTS;

    if ((intsts & (UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk)) != 0U)
    {
        uart_rs485_handle_rx_irq(uart);
    }

    if ((intsts & UART_INTSTS_BUFERRINT_Msk) != 0U)
    {
        uart_rs485_clear_fifo_errors(uart);
        uart->FIFOSTS = UART_FIFOSTS_RXOVIF_Msk | UART_FIFOSTS_TXOVIF_Msk;
    }

    if ((intsts & UART_INTSTS_RLSINT_Msk) != 0U)
    {
        uart_rs485_clear_fifo_errors(uart);
    }
}

void UART0_IRQHandler(void)
{
    if (g_uart_rs485.initialized && (g_uart_rs485.config.uart == UART0))
    {
        uart_rs485_driver_irq_handler(UART0);
    }
    else
    {
        UART_ClearIntFlag(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk | UART_INTSTS_BUFERRINT_Msk | UART_INTSTS_RLSINT_Msk);
    }
}

void uart_rs485_driver_init(const uart_rs485_driver_config_t *config)
{
    if ((config == NULL) || (config->uart == NULL))
    {
        return;
    }

    uart_rs485_driver_uninit();

    g_uart_rs485.config = *config;
    g_uart_rs485.rx_callback = NULL;
    g_uart_rs485.rx_user_data = NULL;
    g_uart_rs485.tx_active = false;
    g_uart_rs485.baudrate = config->baudrate;

    if (config->dir_gpio_port != NULL)
    {
        GPIO_SetMode(config->dir_gpio_port, (uint32_t)(1UL << config->dir_gpio_pin), GPIO_MODE_OUTPUT);
        uart_rs485_set_dir_rx();
    }

    uart_rs485_enable_clock(config->module_clock);
    uart_rs485_configure_uart(config->uart, config->baudrate);

    NVIC_EnableIRQ(config->irq_number);

    g_uart_rs485.initialized = true;
}

void uart_rs485_driver_uninit(void)
{
    if (!g_uart_rs485.initialized)
    {
        return;
    }

    NVIC_DisableIRQ(g_uart_rs485.config.irq_number);
    uart_rs485_disable_uart();
    g_uart_rs485.initialized = false;
    g_uart_rs485.tx_active = false;
    g_uart_rs485.rx_callback = NULL;
    g_uart_rs485.rx_user_data = NULL;
}

bool uart_rs485_driver_set_baudrate(uint32_t baudrate)
{
    if (!g_uart_rs485.initialized || (g_uart_rs485.config.uart == NULL))
    {
        return false;
    }

    g_uart_rs485.baudrate = baudrate;
    uart_rs485_configure_uart(g_uart_rs485.config.uart, baudrate);
    return true;
}

uint32_t uart_rs485_driver_get_baudrate(void)
{
    return g_uart_rs485.baudrate;
}

void uart_rs485_driver_set_rx_callback(uart_rs485_rx_callback_t callback, void *user_data)
{
    g_uart_rs485.rx_callback = callback;
    g_uart_rs485.rx_user_data = user_data;
}

static void uart_rs485_send_byte(UART_T *uart, uint8_t byte)
{
    while (UART_IS_TX_FULL(uart))
    {
    }
    UART_WRITE(uart, byte);
}

bool uart_rs485_driver_write(const uint8_t *data, uint16_t length)
{
    if (!g_uart_rs485.initialized || (data == NULL) || (length == 0U))
    {
        return false;
    }

    UART_T *uart = g_uart_rs485.config.uart;
    uart_rs485_set_dir_tx();
    g_uart_rs485.tx_active = true;

    for (uint16_t i = 0U; i < length; ++i)
    {
        uart_rs485_send_byte(uart, data[i]);
    }

    while (UART_IS_TX_EMPTY(uart) == 0U)
    {
    }

    uart_rs485_set_dir_rx();
    g_uart_rs485.tx_active = false;
    return true;
}

void uart_rs485_driver_flush(void)
{
    if (!g_uart_rs485.initialized || (g_uart_rs485.config.uart == NULL))
    {
        return;
    }

    while (UART_IS_TX_EMPTY(g_uart_rs485.config.uart) == 0U)
    {
    }

    uart_rs485_set_dir_rx();
    g_uart_rs485.tx_active = false;
}

void uart_rs485_driver_get_state(uart_rs485_driver_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    state->initialized = g_uart_rs485.initialized;
    state->tx_active = g_uart_rs485.tx_active;
    state->baudrate = g_uart_rs485.baudrate;
}
