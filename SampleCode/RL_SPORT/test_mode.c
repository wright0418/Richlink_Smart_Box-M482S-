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

static volatile uint8_t g_uart_test_mode = 0u;

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

static void Test_Buzzer(void)
{
    printf("[Test] Buzzer PC7 beep\n");
    BuzzerPlay(1000, 200);
    delay_ms(250);
}

static void Test_Key(void)
{
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
    printf("[Test] ADC PB1 (battery)\n");
    uint16_t raw = Adc_ReadBatteryRawAvg(ADC_BATT_AVG_SAMPLES);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    printf("[Test] raw=%u V=%.2fV (low<=%.2fV)\n",
           (unsigned int)raw,
           vbat,
           (double)ADC_BATT_LOW_V);
}

static void Test_USB(void)
{
    printf("[Test] USB FS HID Mouse: connect to PC, press 'q' to stop\n");
    UsbHidMouse_TestStart();

    uint32_t last_update = get_ticks_ms();
    while (1)
    {
        char c = 0;
        if (UART0_ReadCharNonBlocking(&c))
        {
            if (c == 'q' || c == 'Q' || c == '0' || c == 'x' || c == 'X')
            {
                break;
            }
        }

        if (get_elapsed_ms(last_update) >= 1u)
        {
            last_update = get_ticks_ms();
            UsbHidMouse_TestUpdate();
        }
    }

    UsbHidMouse_TestStop();
    printf("[Test] USB HID stopped\n");
}

static void RunAllTests(void)
{
    Test_LED();
    Test_Buzzer();
    Test_Key();
    Test_Hall();
    Test_Gsensor();
    Test_ADC();
}

static void UART0_TestMenuLoop(void)
{
    char line[8];
    printf("\n=== UART0 Test Mode ===\n");
    printf("1) LED PB3\n");
    printf("2) Buzzer PC7\n");
    printf("3) Key PB15\n");
    printf("4) HALL PB7/PB8\n");
    printf("5) G-sensor I2C\n");
    printf("6) ADC PB1\n");
    printf("7) Run all tests\n");
    printf("8) USB FS HID Mouse\n");
    printf("0) Exit\n");

    while (g_uart_test_mode)
    {
        printf("Select> ");
        UART0_ReadLineBlocking(line, sizeof(line));

        switch (line[0])
        {
        case '1':
            Test_LED();
            break;
        case '2':
            Test_Buzzer();
            break;
        case '3':
            Test_Key();
            break;
        case '4':
            Test_Hall();
            break;
        case '5':
            Test_Gsensor();
            break;
        case '6':
            Test_ADC();
            break;
        case '7':
            RunAllTests();
            break;
        case '8':
            Test_USB();
            break;
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
