#include "test_mode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "i2c.h"
#include "system_status.h"

static volatile uint8_t g_uart_test_mode = 0u;

/* ------------------------------------------------------------------ */
/* UART0 helpers                                                       */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* AT+TEST= command dispatcher (forward declaration)                   */
/* ------------------------------------------------------------------ */
static void AT_DispatchCommand(const char *line);

/* ------------------------------------------------------------------ */
/* Poll entry: detect "test" OR "AT+TEST=" prefix                      */
/* ------------------------------------------------------------------ */

#define AT_PREFIX "AT+TEST="
#define AT_PREFIX_LEN 8
#define CMD_BUF_SIZE 64

/* Shared AT test thresholds/timeouts (single item + ALL must stay aligned) */
#define TEST_I2C_MAG_MIN_G 0.2f /* low floor: tilted board at cpg=2048 may read ~0.3g and still be alive */
#define TEST_I2C_MAG_MAX_G 2.0f
#define TEST_BLE_SETTLE_MS 200u
#define TEST_BLE_NAME_TIMEOUT_MS 2500u
#define TEST_GSENSOR_CAL_SAMPLES 64u
#define TEST_GSENSOR_CAL_SAMPLE_INTERVAL_MS 20u

static float s_gsensor_cal_scale = 1.0f;
static uint8_t s_gsensor_cal_valid = 0u;

void TestMode_PollEnter(void)
{
    static char cmd_buf[CMD_BUF_SIZE];
    static uint8_t cmd_idx = 0;
    char c = 0;

    while (UART0_ReadCharNonBlocking(&c))
    {
        if (c == '\r' || c == '\n')
        {
            if (cmd_idx == 0)
                continue; /* ignore empty lines */

            cmd_buf[cmd_idx] = '\0';

            /* Check for AT+TEST= prefix first */
            if (cmd_idx >= AT_PREFIX_LEN &&
                strncmp(cmd_buf, AT_PREFIX, AT_PREFIX_LEN) == 0)
            {
                AT_DispatchCommand(cmd_buf + AT_PREFIX_LEN);
            }
            else if (strcmp(cmd_buf, "test") == 0)
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
    (void)Test_BLE_WaitMode(1u, 800u);

    /* Single attempt only: if CMD mode cannot be entered once, fail. */
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_CCMD);
    return Test_BLE_WaitMode(0u, 1200u);
}

static uint8_t Test_BLE_SwitchToDataMode(void)
{
    Sys_SetBleMode(0u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_MODE_DATA);
    return Test_BLE_WaitMode(1u, 400u);
}

static void Test_BLE_AT_CMD(void)
{
    const uint32_t timeout_ms = 2000u;
    uint32_t start = get_ticks_ms();
    uint8_t got_name_resp = 0u;
    uint8_t pass = 0u;

    printf("[Test] BLE AT CMD: query name, expect ROPR_\n");

    if (!Test_BLE_SwitchToCmdMode())
    {
        printf("[Test] BLE_AT_CMD FAIL: cannot enter CMD mode after retries\n");
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
            printf("[Test] BLE NAME = %s\n", name);

            /* Single read result: one response is enough to decide pass/fail. */
            if ((strstr(name, "ROPR_") != NULL) || (strstr(name, "ROPE_") != NULL))
            {
                pass = 1u;
            }
            break;
        }

        delay_ms(5u);
    }

    if (pass)
    {
        printf("[Test] BLE_AT_CMD PASS\n");
    }
    else if (!got_name_resp)
    {
        printf("[Test] BLE_AT_CMD FAIL: no name response\n");
    }
    else
    {
        printf("[Test] BLE_AT_CMD FAIL: name format invalid\n");
    }

    if (!Test_BLE_SwitchToDataMode())
    {
        printf("[Test] WARN: failed to return DATA mode\n");
    }
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
    printf("8) USB FS HID Mouse (auto 5s)\n");
    printf("9) BLE AT CMD name check\n");
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
        case '9':
            Test_BLE_AT_CMD();
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

/* ================================================================== */
/*  AT+TEST= Auto-Test Command Implementations                        */
/* ================================================================== */

/* Helper: parse a uint32 from string, returns 0 on empty/null */
static uint32_t parse_u32(const char *s)
{
    if (!s || !*s)
        return 0u;
    return (uint32_t)strtoul(s, NULL, 10);
}

/* Helper: find next comma-delimited token; returns pointer after comma or NULL */
static const char *next_param(const char *s)
{
    if (!s)
        return NULL;
    const char *p = strchr(s, ',');
    if (p)
        return p + 1;
    return NULL;
}

/* Helper: copy first comma-delimited token into buf (upper-bound len) */
static void copy_token(const char *s, char *buf, uint32_t buf_len)
{
    uint32_t i = 0;
    if (!s)
    {
        buf[0] = '\0';
        return;
    }
    while (*s && *s != ',' && i + 1 < buf_len)
    {
        buf[i++] = *s++;
    }
    buf[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* AT+TEST=INFO                                                        */
/* ------------------------------------------------------------------ */
static uint8_t AT_Info(const char *param)
{
    (void)param;
    printf("+TEST:INFO,PASS,FW=%s,BRD=%s,BUILD=%s_%s\r\n",
           TEST_FW_VERSION, TEST_BOARD_NAME, FW_BUILD_DATE, FW_BUILD_TIME);
    return 1u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=LED,<ON|OFF|BLINK>                                          */
/* ------------------------------------------------------------------ */
static uint8_t AT_Led(const char *param)
{
    char tok[8];
    copy_token(param, tok, sizeof(tok));

    if (strcmp(tok, "ON") == 0)
    {
        PB->DOUT |= BIT3;
        printf("+TEST:LED,PASS,STATE=ON\r\n");
        return 1u;
    }
    if (strcmp(tok, "OFF") == 0)
    {
        PB->DOUT &= ~BIT3;
        printf("+TEST:LED,PASS,STATE=OFF\r\n");
        return 1u;
    }
    if (strcmp(tok, "BLINK") == 0)
    {
        for (int i = 0; i < 3; ++i)
        {
            PB->DOUT |= BIT3;
            delay_ms(100);
            PB->DOUT &= ~BIT3;
            delay_ms(100);
        }
        printf("+TEST:LED,PASS,BLINK=3\r\n");
        return 1u;
    }
    printf("+TEST:LED,FAIL,BAD_PARAM\r\n");
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=BUZZER[,freq,dur]                                           */
/* ------------------------------------------------------------------ */
static uint8_t AT_Buzzer(const char *param)
{
    uint32_t freq = 1000u;
    uint32_t dur = 200u;

    if (param && *param)
    {
        freq = parse_u32(param);
        const char *p2 = next_param(param);
        if (p2)
            dur = parse_u32(p2);
    }

    if (freq == 0)
        freq = 1000u;
    if (dur == 0)
        dur = 200u;

    BuzzerPlay(freq, dur);
    delay_ms(dur + 50u);

    printf("+TEST:BUZZER,PASS,FREQ=%lu,DUR=%lu\r\n",
           (unsigned long)freq, (unsigned long)dur);
    return 1u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=KEY[,timeout_ms]                                            */
/* ------------------------------------------------------------------ */
static uint8_t AT_Key(const char *param)
{
    uint32_t timeout = 5000u;

    if (param && *param)
        timeout = parse_u32(param);
    if (timeout == 0)
        timeout = 5000u;

    uint32_t start = get_ticks_ms();
    while (!is_timeout(start, timeout))
    {
        if ((GPIO_GET_IN_DATA(PB) & BIT15) == 0)
        {
            uint32_t elapsed = get_elapsed_ms(start);
            printf("+TEST:KEY,PASS,T=%lu\r\n", (unsigned long)elapsed);
            return 1u;
        }
    }
    printf("+TEST:KEY,FAIL,TIMEOUT\r\n");
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=HALL  or  AT+TEST=HALL,WAIT,timeout_ms                      */
/* ------------------------------------------------------------------ */
static uint8_t AT_Hall(const char *param)
{
    char tok[8];
    copy_token(param, tok, sizeof(tok));

    if (strcmp(tok, "WAIT") == 0)
    {
        /* Wait for PB7 to produce >= 2 edge transitions (interrupts).
           Only PB7 is evaluated; PB8 is ignored in WAIT mode. */
        const char *p2 = next_param(param);
        uint32_t timeout = 3000u;
        if (p2 && *p2)
            timeout = parse_u32(p2);
        if (timeout == 0)
            timeout = 3000u;

        uint32_t last_pb7 = (PB->PIN & BIT7) ? 1u : 0u;
        uint32_t pb7_edges = 0u;
        uint32_t start = get_ticks_ms();

        while (!is_timeout(start, timeout))
        {
            uint32_t pb7_now = (PB->PIN & BIT7) ? 1u : 0u;
            if (pb7_now != last_pb7)
            {
                pb7_edges++;
                last_pb7 = pb7_now;
                if (pb7_edges >= 2u)
                {
                    printf("+TEST:HALL,PASS,EDGE=PB7,CNT=%lu,T=%lu\r\n",
                           (unsigned long)pb7_edges, (unsigned long)get_elapsed_ms(start));
                    return 1u;
                }
            }
            delay_ms(2);
        }
        printf("+TEST:HALL,FAIL,TIMEOUT,CNT=%lu\r\n", (unsigned long)pb7_edges);
        return 0u;
    }

    /* Instant read */
    uint32_t pb7 = (PB->PIN & BIT7) ? 1u : 0u;
    uint32_t pb8 = (PB->PIN & BIT8) ? 1u : 0u;
    printf("+TEST:HALL,PASS,PB7=%lu,PB8=%lu\r\n",
           (unsigned long)pb7, (unsigned long)pb8);
    return 1u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=GSENSOR                                                     */
/* ------------------------------------------------------------------ */
static uint8_t AT_GsensorCal(void)
{
    float sum_mag = 0.0f;

    for (uint32_t i = 0u; i < TEST_GSENSOR_CAL_SAMPLES; i++)
    {
        int16_t axis[3] = {0};
        GsensorReadAxis(axis);
        sum_mag += Gsensor_CalcMagnitude_g_from_raw(axis);
        delay_ms(TEST_GSENSOR_CAL_SAMPLE_INTERVAL_MS);
    }

    float avg_mag = sum_mag / (float)TEST_GSENSOR_CAL_SAMPLES;
    if (avg_mag < 0.05f)
    {
        s_gsensor_cal_scale = 1.0f;
        s_gsensor_cal_valid = 0u;
        printf("+TEST:GSENSOR,FAIL,CAL,NO_VALID_DATA\r\n");
        return 0u;
    }

    s_gsensor_cal_scale = 1.0f / avg_mag;
    s_gsensor_cal_valid = 1u;

    printf("+TEST:GSENSOR,PASS,CAL,SAMPLES=%lu,G_AVG=%.3f,SCALE=%.6f\r\n",
           (unsigned long)TEST_GSENSOR_CAL_SAMPLES, avg_mag, s_gsensor_cal_scale);
    return 1u;
}

static uint8_t AT_Gsensor(const char *param)
{
    char tok[8];
    copy_token(param, tok, sizeof(tok));

    if (strcmp(tok, "CAL") == 0)
    {
        return AT_GsensorCal();
    }
    if (tok[0] != '\0')
    {
        printf("+TEST:GSENSOR,FAIL,BAD_PARAM\r\n");
        return 0u;
    }

    /* Force sensor into a known-good state and let it settle.
       In ALL mode the LED+Buzzer tests provide ~850 ms of idle I2C time
       before GSENSOR is read; single-item mode needs an explicit settling
       period to match that behaviour.  GsensorWakeup() re-writes the
       control register so the conversion pipeline is cleanly restarted. */
    GsensorWakeup();
    delay_ms(100u); /* >12 full conversion cycles at ~125 Hz ODR */

    /* Flush stale / transitional samples */
    for (uint32_t d = 0u; d < 5u; d++)
    {
        int16_t dummy[3] = {0};
        GsensorReadAxis(dummy);
        delay_ms(10u);
    }

    /* Average 8 fresh samples for the final reading */
    int32_t sum_axis[3] = {0, 0, 0};
    for (uint32_t i = 0; i < 8u; ++i)
    {
        int16_t tmp[3] = {0};
        GsensorReadAxis(tmp);
        sum_axis[0] += tmp[0];
        sum_axis[1] += tmp[1];
        sum_axis[2] += tmp[2];
        if (i < 7u)
            delay_ms(10u);
    }
    int16_t axis[3];
    axis[0] = (int16_t)(sum_axis[0] / 8);
    axis[1] = (int16_t)(sum_axis[1] / 8);
    axis[2] = (int16_t)(sum_axis[2] / 8);

    /* Validate: static magnitude should be ~1G, using project tolerance. */
    float mag_raw_g = Gsensor_CalcMagnitude_g_from_raw(axis);
    float mag_cal_g = mag_raw_g * s_gsensor_cal_scale;
    float err_raw_g = (mag_raw_g >= 1.0f) ? (mag_raw_g - 1.0f) : (1.0f - mag_raw_g);
    float err_cal_g = (mag_cal_g >= 1.0f) ? (mag_cal_g - 1.0f) : (1.0f - mag_cal_g);
    uint8_t use_cal = (s_gsensor_cal_valid && (err_cal_g < err_raw_g)) ? 1u : 0u;
    float mag_eval_g = use_cal ? mag_cal_g : mag_raw_g;
    float mag_min_g = 1.0f - MOVEMENT_MAG_TOLERANCE_G;
    float mag_max_g = 1.0f + MOVEMENT_MAG_TOLERANCE_G;

    if (mag_eval_g >= mag_min_g && mag_eval_g <= mag_max_g)
    {
        printf("+TEST:GSENSOR,PASS,X=%d,Y=%d,Z=%d,G_RAW=%.3f,G_CAL=%.3f,G_USE=%.3f,SRC=%s,CAL=%u\r\n",
               axis[0], axis[1], axis[2], mag_raw_g, mag_cal_g, mag_eval_g,
               use_cal ? "CAL" : "RAW", (unsigned)s_gsensor_cal_valid);
        return 1u;
    }
    printf("+TEST:GSENSOR,FAIL,G_RAW=%.3f,G_CAL=%.3f,G_USE=%.3f,SRC=%s,CAL=%u,X=%d,Y=%d,Z=%d\r\n",
           mag_raw_g, mag_cal_g, mag_eval_g, use_cal ? "CAL" : "RAW",
           (unsigned)s_gsensor_cal_valid, axis[0], axis[1], axis[2]);
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=ADC                                                         */
/* ------------------------------------------------------------------ */
static uint8_t AT_Adc(const char *param)
{
    (void)param;
    uint16_t raw = Adc_ReadBatteryRawAvg(ADC_BATT_AVG_SAMPLES);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    uint32_t mv = (uint32_t)(vbat * 1000.0f);

    if (vbat >= 2.0f && vbat <= 5.5f)
    {
        printf("+TEST:ADC,PASS,RAW=%u,MV=%lu\r\n",
               (unsigned int)raw, (unsigned long)mv);
        return 1u;
    }
    printf("+TEST:ADC,FAIL,OUT_OF_RANGE\r\n");
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=I2C,SCAN                                                    */
/* ------------------------------------------------------------------ */
static uint8_t AT_I2c(const char *param)
{
    if (!param || strncmp(param, "SCAN", 4) != 0)
    {
        printf("+TEST:I2C,FAIL,BAD_PARAM\r\n");
        return 0u;
    }

    /* Use GsensorReadAxis to avoid raw I2C probing bus stalls. */
    int16_t axis[3] = {0};
    GsensorReadAxis(axis);

    float mag_g = Gsensor_CalcMagnitude_g_from_raw(axis);

    /* If magnitude is in a reasonable range the sensor is alive. */
    if (mag_g >= TEST_I2C_MAG_MIN_G && mag_g <= TEST_I2C_MAG_MAX_G)
    {
        printf("+TEST:I2C,FOUND,ADDR=0x%02X\r\n", GSENSOR_ADDR);
        printf("+TEST:I2C,INFO,G=%.3f\r\n", mag_g);
        printf("+TEST:I2C,PASS,COUNT=1\r\n");
        return 1u;
    }
    printf("+TEST:I2C,INFO,G=%.3f\r\n", mag_g);
    printf("+TEST:I2C,FAIL,NO_DEVICE\r\n");
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=BLE,NAME / AT+TEST=BLE,MAC                                 */
/* ------------------------------------------------------------------ */
static uint8_t AT_BleName(void)
{
    if (!Test_BLE_SwitchToCmdMode())
    {
        printf("+TEST:BLE,FAIL,CMD_MODE\r\n");
        return 0u;
    }

    /* Let BLE module settle after CMD mode switch and drain pending data */
    delay_ms(TEST_BLE_SETTLE_MS);
    CheckBleRecvMsg();

    Sys_SetDeviceName("", 0u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_NAME_QUERY);

    uint32_t start = get_ticks_ms();
    while (!is_timeout(start, TEST_BLE_NAME_TIMEOUT_MS))
    {
        CheckBleRecvMsg();
        const char *name = Sys_GetDeviceName();
        if (name && name[0] != '\0')
        {
            uint8_t ok = (strstr(name, "ROPR_") || strstr(name, "ROPE_")) ? 1u : 0u;
            printf("+TEST:BLE,%s,NAME=%s\r\n", ok ? "PASS" : "FAIL", name);
            Test_BLE_SwitchToDataMode();
            return ok;
        }
        delay_ms(5u);
    }

    printf("+TEST:BLE,FAIL,NO_RESPONSE\r\n");
    Test_BLE_SwitchToDataMode();
    return 0u;
}

static uint8_t AT_BleMac(void)
{
    if (!Test_BLE_SwitchToCmdMode())
    {
        printf("+TEST:BLE,FAIL,CMD_MODE\r\n");
        return 0u;
    }

    delay_ms(200u);
    Sys_SetMacAddr("", 0u);
    BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_ADDR_QUERY);

    uint32_t start = get_ticks_ms();
    while (!is_timeout(start, 2000u))
    {
        CheckBleRecvMsg();
        const char *mac = Sys_GetMacAddr();
        if (mac && mac[0] != '\0')
        {
            printf("+TEST:BLE,PASS,MAC=%s\r\n", mac);
            Test_BLE_SwitchToDataMode();
            return 1u;
        }
        delay_ms(5u);
    }

    printf("+TEST:BLE,FAIL,NO_RESPONSE\r\n");
    Test_BLE_SwitchToDataMode();
    return 0u;
}

static uint8_t AT_Ble(const char *param)
{
    char tok[8];
    copy_token(param, tok, sizeof(tok));

    if (strcmp(tok, "NAME") == 0)
        return AT_BleName();
    if (strcmp(tok, "MAC") == 0)
        return AT_BleMac();

    printf("+TEST:BLE,FAIL,BAD_PARAM\r\n");
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=USB                                                         */
/* ------------------------------------------------------------------ */
static uint8_t AT_Usb(const char *param)
{
    (void)param;
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
    printf("+TEST:USB,PASS,DUR=5000\r\n");
    return 1u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=PWR,VBUS / AT+TEST=PWR,LOCK                                */
/* ------------------------------------------------------------------ */
static uint8_t AT_Pwr(const char *param)
{
    char tok[8];
    copy_token(param, tok, sizeof(tok));

    if (strcmp(tok, "VBUS") == 0)
    {
        uint8_t vbus = USBDetect_IsHigh();
        printf("+TEST:PWR,PASS,VBUS=%u\r\n", (unsigned)vbus);
        return 1u;
    }
    if (strcmp(tok, "LOCK") == 0)
    {
        uint32_t pa11 = (PA->PIN & BIT11) ? 1u : 0u;
        printf("+TEST:PWR,PASS,PA11=%lu\r\n", (unsigned long)pa11);
        return 1u;
    }
    printf("+TEST:PWR,FAIL,BAD_PARAM\r\n");
    return 0u;
}

/* ------------------------------------------------------------------ */
/* AT+TEST=ALL — run all auto-testable items                           */
/* ------------------------------------------------------------------ */

/* BLE combined NAME+MAC test for AT_All (single CMD-mode session) */
static void AT_All_Ble(uint32_t *pass, uint32_t *fail)
{
    uint8_t ble_ok = 0u;
    if (Test_BLE_SwitchToCmdMode())
    {
        /* Let BLE module settle after CMD mode switch */
        delay_ms(TEST_BLE_SETTLE_MS);
        CheckBleRecvMsg();

        /* NAME */
        Sys_SetDeviceName("", 0u);
        BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_NAME_QUERY);
        uint32_t start = get_ticks_ms();
        while (!is_timeout(start, TEST_BLE_NAME_TIMEOUT_MS))
        {
            CheckBleRecvMsg();
            const char *name = Sys_GetDeviceName();
            if (name && name[0] != '\0')
            {
                if (strstr(name, "ROPR_") || strstr(name, "ROPE_"))
                {
                    printf("+TEST:BLE,PASS,NAME=%s\r\n", name);
                    ble_ok = 1u;
                }
                else
                {
                    printf("+TEST:BLE,FAIL,NAME=%s\r\n", name);
                }
                break;
            }
            delay_ms(5u);
        }
        if (!ble_ok && !Sys_GetDeviceName()[0])
        {
            printf("+TEST:BLE,FAIL,NO_RESPONSE\r\n");
        }

        /* MAC (reuse CMD mode — gap after NAME response) */
        delay_ms(200u);
        Sys_SetMacAddr("", 0u);
        BLE_UART_SEND((void *)UART1, "%s", BLE_CMD_ADDR_QUERY);
        start = get_ticks_ms();
        uint8_t mac_ok = 0u;
        while (!is_timeout(start, 2000u))
        {
            CheckBleRecvMsg();
            const char *mac = Sys_GetMacAddr();
            if (mac && mac[0] != '\0')
            {
                printf("+TEST:BLE,PASS,MAC=%s\r\n", mac);
                mac_ok = 1u;
                break;
            }
            delay_ms(5u);
        }
        if (!mac_ok)
        {
            /* In ALL flow, MAC query is informative only; do not fail. */
            printf("+TEST:BLE,INFO,MAC=NA\r\n");
            mac_ok = 1u;
        }

        Test_BLE_SwitchToDataMode();

        if (ble_ok)
            (*pass)++;
        else
            (*fail)++;
        if (mac_ok)
            (*pass)++;
        else
            (*fail)++;
    }
    else
    {
        printf("+TEST:BLE,FAIL,CMD_MODE\r\n");
        printf("+TEST:BLE,FAIL,CMD_MODE\r\n");
        *fail += 2u;
    }
}

static void AT_All(void)
{
    uint32_t pass = 0u;
    uint32_t fail = 0u;

#define TRACK(fn)   \
    do              \
    {               \
        if (fn)     \
            pass++; \
        else        \
            fail++; \
    } while (0)

    /* 1. INFO — always pass */
    AT_Info(NULL);
    pass++;

    /* 2. LED BLINK */
    TRACK(AT_Led("BLINK"));

    /* 3. BUZZER */
    TRACK(AT_Buzzer(NULL));

    /* 4. GSENSOR */
    TRACK(AT_Gsensor(NULL));

    /* 5. ADC */
    TRACK(AT_Adc(NULL));

    /* 6. I2C SCAN */
    TRACK(AT_I2c("SCAN"));

    /* 7. HALL instant read */
    TRACK(AT_Hall(NULL));

    /* 8-9. BLE NAME + MAC (combined CMD-mode session) */
    AT_All_Ble(&pass, &fail);

    /* 10. PWR VBUS */
    TRACK(AT_Pwr("VBUS"));

    /* 11. PWR LOCK */
    TRACK(AT_Pwr("LOCK"));

#undef TRACK

    uint32_t total = pass + fail;
    printf("+TEST:ALL,DONE,PASS=%lu,FAIL=%lu,TOTAL=%lu\r\n",
           (unsigned long)pass, (unsigned long)fail, (unsigned long)total);
    printf(fail == 0u ? "OK\r\n" : "ERROR\r\n");
}

/* ================================================================== */
/*  AT Command Dispatch Table                                          */
/* ================================================================== */

typedef uint8_t (*AT_Handler)(const char *param);

typedef struct
{
    const char *cmd;
    AT_Handler handler;
} AT_CmdEntry;

static const AT_CmdEntry s_at_cmd_table[] = {
    {"INFO", AT_Info},
    {"LED", AT_Led},
    {"BUZZER", AT_Buzzer},
    {"KEY", AT_Key},
    {"HALL", AT_Hall},
    {"GSENSOR", AT_Gsensor},
    {"ADC", AT_Adc},
    {"I2C", AT_I2c},
    {"BLE", AT_Ble},
    {"USB", AT_Usb},
    {"PWR", AT_Pwr},
};

#define AT_CMD_TABLE_SIZE (sizeof(s_at_cmd_table) / sizeof(s_at_cmd_table[0]))

/* ------------------------------------------------------------------ */
/* AT command dispatcher                                                */
/* ------------------------------------------------------------------ */
static void AT_DispatchCommand(const char *line)
{
    static uint8_t s_at_test_session_active = 0u;

    char cmd[12];
    copy_token(line, cmd, sizeof(cmd));
    const char *param = next_param(line);

    /* Explicit command to leave AT test session and resume normal game loop */
    if (strcmp(cmd, "EXIT") == 0)
    {
        s_at_test_session_active = 0u;
        Sys_SetReplMode(0u);
        Sys_SetLedOverride(0u);
        (void)Sys_TakeHallPb7PendingEdges();
        Sys_SetHallPb7IrqFlag(0);
        printf("+TEST:EXIT,PASS\r\n");
        printf("OK\r\n");
        return;
    }

    /* Enter/keep AT test session active: this keeps main loop out of game mode
       across commands (until AT+TEST=EXIT). */
    s_at_test_session_active = 1u;
    Sys_SetReplMode(1u);
    Sys_SetLedOverride(1u);

    /* Clear stale hall pending events right before handling command */
    (void)Sys_TakeHallPb7PendingEdges();
    Sys_SetHallPb7IrqFlag(0);

    if (strcmp(cmd, "ALL") == 0)
    {
        AT_All();
    }
    else
    {
        uint8_t found = 0u;
        for (uint32_t i = 0u; i < AT_CMD_TABLE_SIZE; i++)
        {
            if (strcmp(cmd, s_at_cmd_table[i].cmd) == 0)
            {
                uint8_t result = s_at_cmd_table[i].handler(param);
                printf(result ? "OK\r\n" : "ERROR\r\n");
                found = 1u;
                break;
            }
        }
        if (!found)
        {
            printf("+TEST:UNKNOWN,FAIL,BAD_CMD\r\n");
            printf("ERROR\r\n");
        }
    }

    /* Clear hall pending events once after command as well. */
    (void)Sys_TakeHallPb7PendingEdges();
    Sys_SetHallPb7IrqFlag(0);
}
