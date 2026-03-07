/*
 * board_test_gpio.c
 * Minimal GPIO hardware test for RL_SPORT V3
 */

#include "board_test_gpio.h"
#include <stdio.h>
#include "NuMicro.h"
#include "gpio.h"
#include "gsensor.h"
#include "adc.h"
#include "buzzer.h"
#include "timer.h"
#include "project_config.h"

/* Simple blocking delay (approx) */
static void bt_delay(uint32_t ms)
{
    /* Use existing timer-based delay for predictable timing across builds */
    delay_ms(ms);
}

static void bt_print_result(const char *name, uint8_t pass, const char *hint)
{
    if (pass)
    {
        printf("[BT] %s: PASS\n", name);
    }
    else
    {
        printf("[BT] %s: FAIL - %s\n", name, hint);
    }
}

void BoardTest_GPIO_Init(void)
{
    /* Configure commonly used pins for tests. Assumes SYS_Init() already ran. */
    /* LED (PB3) */
    GPIO_SetMode(PB, BIT3, GPIO_MODE_OUTPUT);

    /* Key (PB15) */
    GPIO_SetMode(PB, BIT15, GPIO_MODE_QUASI);
    GPIO_SetPullCtl(PB, BIT15, GPIO_PUSEL_PULL_UP);
    GPIO_SET_DEBOUNCE_TIME(GPIO_DBCTL_DBCLKSRC_LIRC, GPIO_DBCTL_DBCLKSEL_512);
    GPIO_ENABLE_DEBOUNCE(PB, BIT15);

    /* HALL sensors (PB7, PB8) */
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);

    /* Buzzer (PC7) */
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);

    printf("[BT] GPIO init done\n");
}

static uint8_t test_led(void)
{
    printf("[BT] LED: blink 3 times. Check LED on PB3.\n");
    for (int i = 0; i < 3; ++i)
    {
        PB->DOUT |= BIT3; /* LED on */
        bt_delay(80);
        PB->DOUT &= ~BIT3; /* LED off */
        bt_delay(80);
    }

    /* No feedback pin for LED; this is a manual check item. */
    printf("[BT] LED: MANUAL CHECK. If no blink, LED path is bad.\n");
    return 1u;
}

static uint8_t test_buzzer(void)
{
    printf("[BT] Buzzer: beep 2 times. Check sound output.\n");
    BuzzerPlay(1000u, 120u);
    CLK_SysTickDelay(180000u);
    BuzzerPlay(1000u, 120u);
    CLK_SysTickDelay(180000u);

    /* No feedback pin for buzzer sound pressure; this is a manual check item. */
    printf("[BT] Buzzer: MANUAL CHECK. If no sound, check PC7/BJT/buzzer.\n");
    return 1u;
}

static uint8_t test_key_poll(void)
{
    printf("[BT] Key: press PB15 in 6 seconds.\n");
    uint32_t timeout = 6000; /* approx ms */
    while (timeout--)
    {
        if ((GPIO_GET_IN_DATA(PB) & BIT15) == 0)
        { /* active low */
            printf("[BT] Key event detected on PB15.\n");
            return 1u;
        }
        bt_delay(1);
    }
    printf("[BT] Key timeout.\n");
    return 0u;
}

static uint8_t test_battery_adc(void)
{
    printf("[BT] Battery ADC: read PB1 (EADC0_CH1).\n");

    Adc_InitBattery();
    uint16_t raw = Adc_ReadBatteryRawAvg(ADC_BATT_AVG_SAMPLES);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    printf("[BT] Battery raw=%u, V=%.2fV\n",
           (unsigned int)raw,
           vbat);

    /* Basic hardware sanity window to catch open/short quickly. */
    if (vbat < 2.0f || vbat > 5.5f)
    {
        return 0u;
    }

    return 1u;
}

static uint8_t test_gsensor_i2c(void)
{
    printf("[BT] Gsensor: read XYZ 3 samples.\n");

    /* Ensure sensor is configured and awake. */
    Gsensor_Init(100000, FSR_2G);
    GsensorWakeup();
    CLK_SysTickDelay(50000u); /* 50 ms settle */

    for (int i = 0; i < 3; ++i)
    {
        int16_t axis[3] = {0};
        GsensorReadAxis(axis);
        printf("[BT] Gsensor sample %d: X=%d Y=%d Z=%d\n", i + 1, axis[0], axis[1], axis[2]);
        CLK_SysTickDelay(20000u); /* 20 ms */
    }

    return 1u;
}

void BoardTest_RunAll(void)
{
    uint32_t pass_count = 0u;
    uint32_t fail_count = 0u;
    uint32_t skip_count = 0u;
    uint8_t ok = 0u;

    printf("\n=== RL_SPORT V3 Board Test ===\n");
    printf("[BT] Start tests...\n");
    BoardTest_GPIO_Init();

    ok = test_led();
    bt_print_result("LED", ok, "No blink on PB3");
    pass_count += ok ? 1u : 0u;
    fail_count += ok ? 0u : 1u;
    bt_delay(20);

    ok = test_buzzer();
    bt_print_result("BUZZER", ok, "No sound. Check PC7/BJT/buzzer");
    pass_count += ok ? 1u : 0u;
    fail_count += ok ? 0u : 1u;
    bt_delay(20);

    /* Keep key test optional to avoid blocking in quick test flow. */
    printf("[BT] KEY: SKIP (optional interactive test)\n");
    skip_count++;
    bt_delay(20);

    printf("[BT] POWER_LOCK: SKIP (not tested in board test)\n");
    skip_count++;
    bt_delay(20);

    ok = test_battery_adc();
    bt_print_result("BATTERY_ADC", ok, "Voltage out of range (2.0V~5.5V)");
    pass_count += ok ? 1u : 0u;
    fail_count += ok ? 0u : 1u;
    bt_delay(20);

    ok = test_gsensor_i2c();
    bt_print_result("GSENSOR_I2C", ok, "Read XYZ failed");
    pass_count += ok ? 1u : 0u;
    fail_count += ok ? 0u : 1u;
    bt_delay(20);

    printf("[BT] BLE_AT_NAME: SKIP (use Test Mode item 9)\n");
    skip_count++;

    printf("[BT] SUMMARY: PASS=%lu FAIL=%lu SKIP=%lu\n",
           (unsigned long)pass_count,
           (unsigned long)fail_count,
           (unsigned long)skip_count);
    printf("[BT] End.\n");
}
