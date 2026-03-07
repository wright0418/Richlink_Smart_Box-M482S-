/**
 * @file main.c
 * @brief Smart Box Main Application
 * @date 2025-10-18
 *
 * Main application for Smart Box with BLE Mesh, LED indicators,
 * Digital I/O control, and Modbus RTU sensor integration.
 */

/* =============================================================================
 * Header Files
 * ===========================================================================*/
#include "NuMicro.h" // IWYU pragma: keep
#include "ble_mesh_at.h"
#include "led_indicator.h"
#include "digital_io.h"
#include "mesh_handler.h"
#include "modbus_sensor_manager.h"
#include "mesh_modbus_agent.h"
#include "modbus_auto_detect.h"
#include "hex_utils.h"
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * Macro Definitions
 * ===========================================================================*/
// System Configuration
#define PLL_CLOCK 192000000
#define PROJECT_NAME "Smart_Box"

// Modbus RTU Configuration
#define MODBUS_SENSOR_SLAVE_ADDRESS (0x01U)
#define MODBUS_SENSOR_START_ADDRESS (0x0000U)
#define MODBUS_SENSOR_REGISTER_QUANTITY (6U)
#define MODBUS_SENSOR_POLL_INTERVAL_MS (1000U)
#define MODBUS_SENSOR_RESPONSE_TIMEOUT_MS (200U)
#define MODBUS_SENSOR_FAILURE_THRESHOLD (3U)

/* =============================================================================
 * Global Variables
 * ===========================================================================*/
// System time counter (incremented in SysTick_Handler)
volatile uint32_t g_systick_ms = 0;

/* =============================================================================
 * Static Variables
 * ===========================================================================*/
// BLE Mesh AT controller
static ble_mesh_at_controller_t s_ble_at;

// Device UID storage
static char s_device_uid[32];

// Modbus sensor manager and agent
static modbus_sensor_manager_t s_modbus_sensor_manager;
static mesh_modbus_agent_t s_mesh_modbus_agent;

/* =============================================================================
 * Static Function Declarations
 * ===========================================================================*/
// System initialization
static void sys_init(void);

// Callbacks - BLE Mesh
static void on_ble_mesh_at_event(ble_mesh_at_event_t event, const char *data);

// Callbacks - Mesh Handler
static void mesh_set_binding_state_callback(bool bound);
static void mesh_set_yellow_led_callback(bool on);

// Callbacks - Digital IO
static void send_nr_command(void);
static void on_di_changed_callback(bool new_state);

// Callbacks - Modbus Sensor Manager
static void modbus_sensor_success_callback(const uint16_t *registers, uint16_t quantity);
static void modbus_sensor_error_callback(modbus_exception_t exception, uint32_t consecutive_failures);

// Callbacks - Mesh Modbus Agent
static void agent_response_ready_callback(const uint8_t *data, uint8_t length);
static void agent_error_callback(uint8_t error_code);
static void agent_mesh_data_callback(const uint8_t *data, uint8_t length);
static void process_pending_agent_request(void);

/* =============================================================================
 * Public Utility Functions
 * ===========================================================================*/
/**
 * @brief Get system time in milliseconds
 * @return System time in milliseconds since startup
 */
uint32_t get_system_time_ms(void)
{
    return g_systick_ms;
}

/**
 * @brief Get system time in milliseconds (callback wrapper)
 * @param context Unused context pointer
 * @return System time in milliseconds since startup
 */
static uint32_t get_system_time_ms_cb(void *context)
{
    (void)context;
    return g_systick_ms;
}

/* =============================================================================
 * Interrupt Service Routines
 * ===========================================================================*/
/**
 * @brief SysTick interrupt handler (1ms tick)
 */
void SysTick_Handler(void)
{
    g_systick_ms++;
}

/**
 * @brief UART1 interrupt handler for BLE Mesh AT module
 */
void UART1_IRQHandler(void)
{
    ble_mesh_at_uart_irq_handler(&s_ble_at);
}

/* =============================================================================
 * System Initialization
 * ===========================================================================*/
/**
 * @brief System initialization (clock, UART, GPIO)
 */
static void sys_init(void)
{
    SYS_UnlockReg();

    // Disable PF.2 and PF.3 digital input path
    PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);

    // Enable HXT clock
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    // Set core clock to 192MHz
    CLK_SetCoreClock(PLL_CLOCK);
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);

    // Enable UART0 module clock
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HXT, CLK_CLKDIV0_UART0(1));
    SystemCoreClockUpdate();

    // Set UART0 multi-function pins (PD2=RXD, PD3=TXD for MODBUS RTU)
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD2MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD2MFP_UART0_RXD | SYS_GPD_MFPL_PD3MFP_UART0_TXD);

    // Set PB14 as GPIO (RS485 direction control)
    SYS->GPB_MFPH &= ~SYS_GPB_MFPH_PB14MFP_Msk;
    SYS->GPB_MFPH |= SYS_GPB_MFPH_PB14MFP_GPIO;

    SYS_LockReg();
}

/* =============================================================================
 * BLE Mesh Callbacks
 * ===========================================================================*/
/**
 * @brief Send unbind command (KeyA long press callback)
 */
static void send_nr_command(void)
{
    (void)ble_mesh_at_send_nr(&s_ble_at);
}

/**
 * @brief BLE Mesh AT event callback handler
 * @param event Event type
 * @param data Event data string
 */
static void on_ble_mesh_at_event(ble_mesh_at_event_t event, const char *data)
{
    // Forward event to mesh handler
    mesh_handler_event(event, data);

    // Handle specific events in main
    switch (event)
    {
    case BLE_MESH_AT_EVENT_PROV_BOUND:
        // Store device UID
        {
            const char *uid = ble_mesh_at_get_uid(&s_ble_at);
            size_t k = 0;
            while (uid[k] && k < sizeof(s_device_uid) - 1)
            {
                s_device_uid[k] = uid[k];
                k++;
            }
            s_device_uid[k] = '\0';
        }
        // Update LED state
        led_set_binding_state(true);
        led_set_provisioning_wait(false);
        break;

    case BLE_MESH_AT_EVENT_PROV_UNBOUND:
        s_device_uid[0] = '\0';
        led_set_binding_state(false);
        led_set_provisioning_wait(true);
        break;

    default:
        break;
    }
}

/* =============================================================================
 * Mesh Handler Callbacks
 * ===========================================================================*/
/**
 * @brief Set binding state LED callback
 */
static void mesh_set_binding_state_callback(bool bound)
{
    led_set_binding_state(bound);
}

/**
 * @brief Set yellow LED callback
 */
static void mesh_set_yellow_led_callback(bool on)
{
    led_set_yellow_status(on);
}

/* =============================================================================
 * Digital I/O Callbacks
 * ===========================================================================*/
/**
 * @brief Digital input changed callback - report via BLE Mesh
 * @param new_state New DI state (true=high, false=low)
 *
 * Sends: Header + GET(TYPE=0x00) + STATUS_OK(0x80) + IO_ADDR(0x00) + DI
 */
static void on_di_changed_callback(bool new_state)
{
    uint8_t payload[6];
    uint8_t idx = 0;

    payload[idx++] = 0x82;                    // Header byte1
    payload[idx++] = 0x76;                    // Header byte2
    payload[idx++] = 0x00;                    // TYPE: GET
    payload[idx++] = 0x80;                    // STATUS_OK
    payload[idx++] = 0x00;                    // IO_ADDR
    payload[idx++] = new_state ? 0x01 : 0x00; // DI state

    // Convert to hex string and send via AT+MDTS
    char hex_string[16];
    if (bytes_to_hex(payload, idx, hex_string, sizeof(hex_string)) > 0)
    {
        char mdts_cmd[32];
        snprintf(mdts_cmd, sizeof(mdts_cmd), "AT+MDTS 0 %s", hex_string);
        ble_mesh_at_send_command(&s_ble_at, mdts_cmd);
    }
}

/* =============================================================================
 * Modbus Sensor Manager Callbacks
 * ===========================================================================*/
/**
 * @brief Modbus sensor read success callback
 */
static void modbus_sensor_success_callback(const uint16_t *registers, uint16_t quantity)
{
    (void)registers;
    (void)quantity;
}

/**
 * @brief Modbus sensor read error callback
 */
static void modbus_sensor_error_callback(modbus_exception_t exception, uint32_t consecutive_failures)
{
    (void)exception;
    (void)consecutive_failures;
}

/* =============================================================================
 * Mesh Modbus Agent Callbacks
 * ===========================================================================*/
/**
 * @brief Agent response ready callback - send via BLE Mesh
 * @param data Response data bytes
 * @param length Response data length
 */
static void agent_response_ready_callback(const uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0)
    {
        return;
    }

    // Convert bytes to hex string
    char hex_string[80]; // Max 40 bytes * 2 = 80 chars
    if (bytes_to_hex(data, length, hex_string, sizeof(hex_string)) == 0)
    {
        return;
    }

    // Send via AT+MDTS command
    char mdts_cmd[100];
    snprintf(mdts_cmd, sizeof(mdts_cmd), "AT+MDTS 0 %s", hex_string);
    ble_mesh_at_send_command(&s_ble_at, mdts_cmd);
}

/**
 * @brief Agent error callback
 */
static void agent_error_callback(uint8_t error_code)
{
    (void)error_code;
}

/**
 * @brief Agent mesh data callback - process incoming requests
 * @param data Incoming mesh data
 * @param length Data length
 */
static void agent_mesh_data_callback(const uint8_t *data, uint8_t length)
{
    bool processed = mesh_modbus_agent_process_mesh_data(&s_mesh_modbus_agent, data, length);

    // Buffer request if agent is busy
    if (!processed && mesh_modbus_agent_is_busy(&s_mesh_modbus_agent))
    {
        const mesh_handler_state_t *state = mesh_handler_get_state();
        if (!state->agent_request_pending)
        {
            mesh_handler_state_t *mutable_state = (mesh_handler_state_t *)state;
            if (length <= sizeof(mutable_state->pending_agent_data))
            {
                for (uint8_t i = 0; i < length; i++)
                {
                    mutable_state->pending_agent_data[i] = data[i];
                }
                mutable_state->pending_agent_length = length;
                mutable_state->agent_request_pending = true;
            }
        }
        else
        {
            // Drop request if buffer is full
            mesh_handler_state_t *mutable_state = (mesh_handler_state_t *)state;
            mutable_state->agent_request_dropped++;
        }
    }
}

/**
 * @brief Handle pending agent request from mesh handler queue
 */
static void process_pending_agent_request(void)
{
    if (!mesh_handler_has_pending_agent_request() || mesh_modbus_agent_is_busy(&s_mesh_modbus_agent))
    {
        return;
    }

    uint8_t pending_data[64];
    uint8_t pending_length = 0;
    if (mesh_handler_get_pending_agent_request(pending_data, &pending_length))
    {
        mesh_modbus_agent_process_mesh_data(&s_mesh_modbus_agent, pending_data, pending_length);
    }
}

/* =============================================================================
 * Main Function
 * ===========================================================================*/
/**
 * @brief Main application entry point
 */
int main(void)
{
    // System initialization
    sys_init();
    digital_io_init();

    // Set digital I/O callbacks
    digital_io_set_key_callback(send_nr_command);
    digital_io_set_di_callback(on_di_changed_callback);

    // Initialize LED indicator
    led_indicator_init();

    // Initialize Mesh Handler callbacks
    mesh_handler_callbacks_t mesh_callbacks = {
        .led_yellow = mesh_set_yellow_led_callback,
        .led_pulse_blue = NULL,
        .led_pulse_red = NULL,
        .led_binding = mesh_set_binding_state_callback,
        .led_flash = NULL,
        .pa6_control = NULL,
        .agent_response = agent_mesh_data_callback};
    mesh_handler_init(&mesh_callbacks);

    // Configure SysTick for 1ms interrupt
    SysTick_Config(SystemCoreClock / 1000);

    // === Auto-detect Modbus RTU device ===
    modbus_detect_config_t detect_config;
    modbus_auto_detect_get_default_config(&detect_config);

    modbus_detect_result_t detect_result;
    bool scan_success = modbus_auto_detect_scan(
        &detect_result,
        &detect_config,
        get_system_time_ms_cb,
        NULL,
        NULL);

    // Determine parameters to use
    uint8_t slave_address;
    uint32_t baudrate;

    if (scan_success && detect_result.device_found)
    {
        // Device found - use detected parameters
        slave_address = detect_result.slave_address;
        baudrate = detect_result.baudrate;
        // No LED flash (device found successfully)
    }
    else if (scan_success)
    {
        // Scan completed but no device found - use defaults
        slave_address = MODBUS_SENSOR_SLAVE_ADDRESS;
        baudrate = 9600;
        led_flash_red(3); // Flash red LED 3 times
    }
    else
    {
        // Scan failed - use defaults
        slave_address = MODBUS_SENSOR_SLAVE_ADDRESS;
        baudrate = 9600;
        led_flash_red(5); // Flash red LED 5 times
    }

    // Initialize Modbus Sensor Manager with detected/default parameters
    modbus_sensor_config_t sensor_config = {
        .slave_address = slave_address,
        .start_address = MODBUS_SENSOR_START_ADDRESS,
        .register_quantity = MODBUS_SENSOR_REGISTER_QUANTITY,
        .poll_interval_ms = MODBUS_SENSOR_POLL_INTERVAL_MS,
        .response_timeout_ms = MODBUS_SENSOR_RESPONSE_TIMEOUT_MS,
        .failure_threshold = MODBUS_SENSOR_FAILURE_THRESHOLD};

    if (!modbus_sensor_manager_init(&s_modbus_sensor_manager, &sensor_config,
                                    modbus_sensor_success_callback,
                                    modbus_sensor_error_callback))
    {
        led_red_on();
    }

    // Update baudrate after initialization
    if (baudrate != 9600)
    {
        modbus_sensor_manager_set_baudrate(&s_modbus_sensor_manager, baudrate);
    }

    // Initialize Mesh Modbus Agent
    mesh_modbus_agent_config_t agent_config = {
        .mode = MESH_MODBUS_AGENT_MODE_RL,
        .modbus_timeout_ms = 300,
        .max_response_wait_ms = 500};

    if (!mesh_modbus_agent_init(&s_mesh_modbus_agent, &agent_config,
                                &s_modbus_sensor_manager.client,
                                agent_response_ready_callback,
                                agent_error_callback,
                                NULL))
    {
        led_red_on();
    }

    // Initialize BLE Mesh AT module
    ble_mesh_at_config_t ble_config = {
        .baudrate = 115200,
        .tx_pin_port = 0, // PA
        .tx_pin_num = 9,  // PA.9
        .rx_pin_port = 0, // PA
        .rx_pin_num = 8   // PA.8
    };
    ble_mesh_at_init(&s_ble_at, &ble_config, on_ble_mesh_at_event, get_system_time_ms);
    led_yellow_off();

    // Send reboot command and enter provisioning state
    (void)ble_mesh_at_send_reboot(&s_ble_at);
    s_device_uid[0] = '\0';
    led_set_binding_state(false);
    led_set_provisioning_wait(true);

    // Main loop
    while (1)
    {
        uint32_t current_time = g_systick_ms;

        // Update digital I/O (keys and DI/DO)
        digital_io_update(current_time);

        // Update LED indicators
        led_indicator_update(current_time);

        mesh_modbus_agent_poll(&s_mesh_modbus_agent, current_time);

        // Process pending agent requests
        process_pending_agent_request();

        // Update BLE Mesh AT module
        ble_mesh_at_update(&s_ble_at);
    }
}
