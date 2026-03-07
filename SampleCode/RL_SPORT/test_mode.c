#include "test_mode.h"
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"
#include "project_config.h"
#include "timer.h"
#include "led.h"
#include "buzzer.h"
#include "gsensor.h"
#include "adc.h"
#include "gpio.h"
#include "usb_hid_mouse.h"
#include "ble.h"
#include "system_status.h"

static volatile uint8_t g_uart_test_mode = 0u;

/*
 * Keep legacy/extended test code in source, but hide from current UART menu.
 * 0: hide advanced items (LED / USB / RunAll / etc.)
 * 1: show advanced items
 */
#define TESTMODE_ENABLE_ADVANCED_ITEMS 0u

static int UART0_ReadCharNonBlocking(char *out)
{
    if (UART_IS_RX_READY(UART0))
    {
        *out = (char)UART_READ(UART0);
        return 1;
    }
    return 0;
}

static void UART0_ReadLineBlocking(char *buf, uint32_t buf_len)
{
    uint32_t idx = 0;
    while (1)
    {
        char c = 0;
        if (UART0_ReadCharNonBlocking(&c))
        {
            if (c == '\r' || c == '\n')
            {
                if (idx < buf_len)
                {
                    buf[idx] = '\0';
                }
                return;
            }
            if (idx + 1 < buf_len)
            {
                buf[idx++] = c;
            }
        }
    }
}

void TestMode_PollEnter(void)
{
    static char cmd_buf[8];
    static uint8_t cmd_idx = 0;
    char c = 0;

    while (UART0_ReadCharNonBlocking(&c))
    {
        if (c == '\r' || c == '\n')
        {
            cmd_buf[cmd_idx] = '\0';
            if (strcmp(cmd_buf, "test") == 0)
            {
                g_uart_test_mode = 1u;
            }
            cmd_idx = 0;
            cmd_buf[0] = '\0';
            return;
        }

        if (cmd_idx + 1 < sizeof(cmd_buf))
        {
            cmd_buf[cmd_idx++] = c;
        }
    }
}

#if TESTMODE_ENABLE_ADVANCED_ITEMS
static void Test_LED(void)
{
    printf("[Test] LED PB3 blink x3\n");
    for (int i = 0; i < 3; ++i)
    {
        PB->DOUT |= BIT3;
        delay_ms(100);
        PB->DOUT &= ~BIT3;
        delay_ms(100);
    }
}
#endif

static void Test_Buzzer(void)
{
    printf("\n==================== [RAW] BUZZER ====================\n");
    printf("[Test] Buzzer PC7 beep\n");
    BuzzerPlay(1000, 200);
    delay_ms(250);
}

static void Test_Key(void)
{
    printf("\n===================== [RAW] KEY ======================\n");
    printf("[Test] Key PB15: press within 5s\n");
    uint32_t start = get_ticks_ms();
    while (!is_timeout(start, 5000))
    {
        if ((GPIO_GET_IN_DATA(PB) & BIT15) == 0)
        {
            printf("[Test] Key pressed\n");
            return;
        }
    }
    printf("[Test] Key TIMEOUT\n");
}

static void Test_Hall(void)
{
    printf("\n==================== [RAW] HALL ======================\n");
    printf("[Test] HALL PB7/PB8: read for 3s\n");
    uint32_t start = get_ticks_ms();
    uint32_t last_pb7 = (PB->PIN & BIT7) ? 1u : 0u;
    uint32_t last_pb8 = (PB->PIN & BIT8) ? 1u : 0u;
    while (!is_timeout(start, 3000))
    {
        uint32_t pb7 = (PB->PIN & BIT7) ? 1u : 0u;
        uint32_t pb8 = (PB->PIN & BIT8) ? 1u : 0u;
        if (pb7 != last_pb7 || pb8 != last_pb8)
        {
            printf("[Test] PB7=%lu PB8=%lu\n", (unsigned long)pb7, (unsigned long)pb8);
            last_pb7 = pb7;
            last_pb8 = pb8;
        }
        delay_ms(50);
    }
}

static void Test_Gsensor(void)
{
    printf("\n=================== [RAW] GSENSOR ====================\n");
    printf("[Test] G-sensor I2C read (3 samples)\n");
    for (int i = 0; i < 3; ++i)
    {
        int16_t axis[3] = {0};
        GsensorReadAxis(axis);
        printf("[Test] XYZ = %d, %d, %d\n", axis[0], axis[1], axis[2]);
        delay_ms(50);
    }
}

static void Test_ADC(void)
{
    printf("\n===================== [RAW] ADC ======================\n");
    printf("[Test] ADC PB1 (battery)\n");
    uint16_t raw = Adc_ReadBatteryRawAvg(ADC_BATT_AVG_SAMPLES);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    printf("[Test] raw=%u V=%.2fV (low<=%.2fV)\n",
           (unsigned int)raw,
           vbat,
           (double)ADC_BATT_LOW_V);
}

#if TESTMODE_ENABLE_ADVANCED_ITEMS
static void Test_USB(void)
{
    printf("[Test] USB FS HID Mouse auto test: 5s\n");
    UsbHidMouse_TestStart();

    uint32_t start = get_ticks_ms();
    uint32_t last_update = get_ticks_ms();
    while (!is_timeout(start, 5000))
    {
        if (get_elapsed_ms(last_update) >= 1u)
        {
            last_update = get_ticks_ms();
            UsbHidMouse_TestUpdate();
        }
    }

    UsbHidMouse_TestStop();
    printf("[Test] USB auto test done\n");
}
#endif

static uint8_t Test_BLE_WaitMode(uint8_t target_mode, uint32_t timeout_ms)
{
    uint32_t start = get_ticks_ms();
    while (!is_timeout(start, timeout_ms))
    {
        CheckBleRecvMsg();
        if (Sys_GetBleMode() == target_mode)
        {
            return 1u;
        }
        delay_ms(5u);
    }
    return 0u;
}

static uint8_t Test_BLE_SwitchToCmdMode(void)
{
    Sys_SetBleMode(0u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_DLPS_OFF);
    delay_ms(60u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_ADVERT_ON);
    delay_ms(30u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_MODE_DATA);
    (void)Test_BLE_WaitMode(1u, 500u);

    /* Single attempt only: if CMD mode cannot be entered once, fail. */
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_CCMD);
    return Test_BLE_WaitMode(0u, 700u);
}

static uint8_t Test_BLE_SwitchToDataMode(void)
{
    Sys_SetBleMode(0u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_MODE_DATA);
    return Test_BLE_WaitMode(1u, 400u);
}

static void Test_BLE_AT_CMD(void)
{
    const uint32_t timeout_ms = 1000u;
    uint32_t start = get_ticks_ms();
    uint8_t got_name_resp = 0u;

    printf("\n===================== [RAW] BLE ======================\n");
    printf("[Test] BLE AT CMD: query device name (raw response)\n");

    if (!Test_BLE_SwitchToCmdMode())
    {
        printf("[Test] BLE RAW: cannot enter CMD mode\n");
        return;
    }

    Sys_SetDeviceName("", 0u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_NAME_QUERY);

    while (!is_timeout(start, timeout_ms))
    {
        CheckBleRecvMsg();

        const char *name = Sys_GetDeviceName();
        if (name && name[0] != '\0')
        {
            got_name_resp = 1u;
            printf("[Test] BLE RAW NAME = %s\n", name);
            break;
        }

        delay_ms(5u);
    }

    if (!got_name_resp)
    {
        printf("[Test] BLE RAW: no name response within %lums\n", (unsigned long)timeout_ms);
    }

    if (!Test_BLE_SwitchToDataMode())
    {
        printf("[Test] WARN: failed to return DATA mode\n");
    }
}

#if TESTMODE_ENABLE_ADVANCED_ITEMS
static void RunAllTests(void)
{
    Test_LED();
    Test_Buzzer();
    Test_Key();
    Test_Hall();
    Test_Gsensor();
    Test_ADC();
}
#endif

static void UART0_TestMenuLoop(void)
{
    char line[8];
    printf("\n=== UART0 Test Mode ===\n");
    printf("1) Buzzer PC7\n");
    printf("2) Key PB15\n");
    printf("3) HALL PB7/PB8\n");
    printf("4) G-sensor I2C\n");
    printf("5) ADC PB1\n");
    printf("6) BLE AT CMD name query (raw)\n");
    printf("0) Exit\n");

    while (g_uart_test_mode)
    {
        printf("Select> ");
        UART0_ReadLineBlocking(line, sizeof(line));

        switch (line[0])
        {
        case '1':
            Test_Buzzer();
            break;
        case '2':
            Test_Key();
            break;
        case '3':
            Test_Hall();
            break;
        case '4':
            Test_Gsensor();
            break;
        case '5':
            Test_ADC();
            break;
        case '6':
            Test_BLE_AT_CMD();
            break;
#if TESTMODE_ENABLE_ADVANCED_ITEMS
        case '7':
            RunAllTests();
            break;
        case '8':
            Test_USB();
            break;
#endif
        case '0':
            g_uart_test_mode = 0u;
            printf("Exit test mode\n");
            break;
        default:
            printf("Unknown option\n");
            break;
        }
    }
}

void TestMode_RunMenuIfActive(void)
{
    if (g_uart_test_mode)
    {
        UART0_TestMenuLoop();
    }
}
