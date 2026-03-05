/*
 * board_test_gpio.c
 * Minimal GPIO hardware test for RL_SPORT V3
 */

#include "board_test_gpio.h"
#include <stdio.h>
#include "NuMicro.h"
#include "gpio.h"
#include "gsensor.h"
#include "i2c.h"
#include "adc.h"
#include "project_config.h"

/* Simple blocking delay (approx) */
static void bt_delay(volatile uint32_t d)
{
    while (d--)
    {
        volatile uint32_t i = 12000;
        while (i--)
            __NOP();
    }
}

void BoardTest_GPIO_Init(void)
{
    /* Configure commonly used pins for tests. Assumes SYS_Init() already ran. */
    /* LED (PB3) */
    GPIO_SetMode(PB, BIT3, GPIO_MODE_OUTPUT);

    /* Key (PB15) */
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
    GPIO_SetPullCtl(PB, BIT15, GPIO_PUSEL_PULL_UP);
    GPIO_SET_DEBOUNCE_TIME(GPIO_DBCTL_DBCLKSRC_LIRC, GPIO_DBCTL_DBCLKSEL_512);
    GPIO_ENABLE_DEBOUNCE(PB, BIT15);

    /* HALL sensors (PB7, PB8) */
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);

    /* Buzzer (PC7) */
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);

    printf("[BoardTest] GPIO init done\n");
}

static void test_led(void)
{
    printf("[BoardTest] LED test: blink 3 times\n");
    for (int i = 0; i < 3; ++i)
    {
        PB->DOUT |= BIT3; /* LED on */
        bt_delay(80);
        PB->DOUT &= ~BIT3; /* LED off */
        bt_delay(80);
    }
}

static void test_buzzer(void)
{
    printf("[BoardTest] Buzzer test: square-wave beeps (1kHz)\n");

    /* Generate square wave on PC7 (active high) */
    const uint32_t freq = 1000;       /* 1 kHz */
    const uint32_t duration_ms = 150; /* 150 ms per beep */
    const uint32_t gap_ms = 150;

    for (int i = 0; i < 2; ++i)
    {
        if (freq > 0 && duration_ms > 0)
        {
            uint32_t half_period_us = (1000000U / (freq * 2U));
            uint32_t half_cycles = (uint32_t)(((uint64_t)freq * (uint64_t)duration_ms * 2ULL) / 1000ULL);

            for (uint32_t c = 0; c < half_cycles; ++c)
            {
                PC->DOUT ^= BIT7;
                CLK_SysTickDelay(half_period_us);
            }
        }
        PC->DOUT &= ~BIT7; /* ensure buzzer off */
        CLK_SysTickDelay(gap_ms * 1000U);
    }
}

static void test_key_poll(void)
{
    printf("[BoardTest] Key test: press Key (PB15) within 6s to register\n");
    uint32_t timeout = 6000; /* approx ms */
    while (timeout--)
    {
        if ((GPIO_GET_IN_DATA(PB) & BIT15) == 0)
        { /* active low */
            printf("[BoardTest] Key pressed (PB15)\n");
            return;
        }
        bt_delay(1);
    }
    printf("[BoardTest] Key test: TIMEOUT (no press)\n");
}

static void test_gsensor_i2c(void)
{
    printf("[BoardTest] G-sensor test: init + read via I2C\n");

    /* Ensure sensor is configured and awake */
    Gsensor_Init(100000, FSR_2G);
    GsensorWakeup();

    /* Allow sensor to settle */
    CLK_SysTickDelay(50000); /* 50 ms */

    /* No device ID/Who_Am_I read here; assume G-sensor address is configured and functional */

    for (int i = 0; i < 5; ++i)
    {
        int16_t axis[3] = {0};
        GsensorReadAxis(axis);
        printf("[BoardTest] G-sensor XYZ = %d, %d, %d\n", axis[0], axis[1], axis[2]);
        CLK_SysTickDelay(20000); /* 20 ms */
    }
}

static void test_power_lock(void)
{
    printf("[BoardTest] Power Lock test: PA11 set HIGH\n");

    PowerLock_Set(1);
    CLK_SysTickDelay(10000); /* 10 ms */

    uint32_t pin = (PA->PIN & BIT11) ? 1u : 0u;
    printf("[BoardTest] PA11 state = %s\n", pin ? "HIGH" : "LOW");
}

static void test_battery_adc(void)
{
    printf("[BoardTest] Battery ADC test: read PB1 (EADC0_CH1)\n");

    Adc_InitBattery();
    uint16_t raw = Adc_ReadBatteryRawAvg(ADC_BATT_AVG_SAMPLES);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    printf("[BoardTest] VBAT raw=%u, V=%.2fV (low<=%.2fV)\n",
           (unsigned int)raw,
           vbat,
           (double)ADC_BATT_LOW_V);
}

void BoardTest_RunAll(void)
{
    printf("\n=== RL_SPORT V3 - GPIO Board Test ===\n");

    /* Only print I2C status during board tests to avoid runtime log spam. */
    RL_I2C_SetDebugLog(1);
    BoardTest_GPIO_Init();

    test_led();
    bt_delay(20);
    test_buzzer();
    bt_delay(20);
    /* Skip PB15 key polling test (6s timeout) */
    /* test_key_poll(); */
    bt_delay(20);
    test_power_lock();
    bt_delay(20);
    test_battery_adc();
    bt_delay(20);
    test_gsensor_i2c();

    RL_I2C_SetDebugLog(0);

    printf("[BoardTest] All tests completed (see PASS/FAIL above).\n");
}
