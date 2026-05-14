/*
 * board/board_test_gpio.c
 * Full board-level hardware test for RL_SPORT V3
 */

#include "board_test_gpio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

#include "NuMicro.h"
#include "gpio.h"
#include "../drivers/led.h"
#include "../drivers/ws2812b.h"
#include "../drivers/gsensor.h"
#include "../drivers/adc.h"
#include "../drivers/buzzer.h"
#include "../drivers/timer.h"
#include "../project_config.h"

#define BT_KEY_TIMEOUT_MS 3000u
#define BT_HALL_TIMEOUT_MS 3000u
#define BT_HALL_POLL_MS 5u
#define BT_WS2812_STEP_MS 350u
#define BT_IMU_STATIC_SAMPLES 20u
#define BT_IMU_STATIC_ACC_MEAN_MIN_G 0.50f
#define BT_IMU_STATIC_ACC_MEAN_MAX_G 1.60f
#define BT_IMU_STATIC_ACC_STD_MAX_G 0.08f
#define BT_IMU_STATIC_GYRO_ABS_MEAN_MAX 2500u

typedef enum
{
    BT_STATUS_PASS = 0,
    BT_STATUS_FAIL,
    BT_STATUS_MANUAL,
    BT_STATUS_SKIP
} BoardTestStatus;

typedef struct
{
    uint32_t pass_count;
    uint32_t fail_count;
    uint32_t manual_count;
    uint32_t skip_count;
} BoardTestStats;

/* Simple blocking delay wrapper using existing timer base. */
static void bt_delay(uint32_t ms)
{
    delay_ms(ms);
}

static void bt_record(BoardTestStats *stats,
                      const char *name,
                      BoardTestStatus status,
                      const char *detail)
{
    const char *status_text = "INFO";

    if (stats == NULL)
    {
        return;
    }

    switch (status)
    {
    case BT_STATUS_PASS:
        status_text = "PASS";
        stats->pass_count++;
        break;
    case BT_STATUS_FAIL:
        status_text = "FAIL";
        stats->fail_count++;
        break;
    case BT_STATUS_MANUAL:
        status_text = "MANUAL";
        stats->manual_count++;
        break;
    case BT_STATUS_SKIP:
    default:
        status_text = "SKIP";
        stats->skip_count++;
        break;
    }

    if ((detail != NULL) && (detail[0] != '\0'))
    {
        printf("[BT] %s: %s - %s\n", name, status_text, detail);
    }
    else
    {
        printf("[BT] %s: %s\n", name, status_text);
    }
}

static void bt_set_leds(uint8_t yellow_on, uint8_t green_on)
{
    if (yellow_on != 0u)
    {
        PB->DOUT |= BIT2;
    }
    else
    {
        PB->DOUT &= ~BIT2;
    }

    if (green_on != 0u)
    {
        PB->DOUT |= BIT3;
    }
    else
    {
        PB->DOUT &= ~BIT3;
    }
}

static uint8_t bt_wait_active_low(GPIO_T *port, uint32_t mask, uint32_t timeout_ms)
{
    uint32_t start = get_ticks_ms();

    while (!is_timeout(start, timeout_ms))
    {
        if ((GPIO_GET_IN_DATA(port) & mask) == 0u)
        {
            return 1u;
        }
        bt_delay(1u);
    }

    return 0u;
}

static void bt_fill_rainbow_by_16leds(void)
{
    static const WS2812B_Color k_rainbow[] = {
        {255u, 0u, 0u},
        {255u, 64u, 0u},
        {255u, 128u, 0u},
        {255u, 255u, 0u},
        {0u, 255u, 0u},
        {0u, 255u, 128u},
        {0u, 255u, 255u},
        {0u, 128u, 255u},
        {0u, 0u, 255u},
        {64u, 0u, 255u},
        {128u, 0u, 255u},
        {255u, 0u, 255u},
        {255u, 0u, 128u},
        {255u, 0u, 64u},
        {255u, 32u, 32u},
        {255u, 255u, 255u}};
    const uint8_t color_count = (uint8_t)(sizeof(k_rainbow) / sizeof(k_rainbow[0]));

    for (uint16_t i = 0u; i < (uint16_t)MOLE_WS2812_LED_COUNT; i++)
    {
        uint8_t color_idx = (uint8_t)((i / 16u) % color_count);
        WS2812B_Color color = k_rainbow[color_idx];
        WS2812B_SetPixel(i, color.r, color.g, color.b);
    }
}

void BoardTest_GPIO_Init(void)
{
    /* Output indicators */
    GPIO_SetMode(PB, BIT2 | BIT3, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);

    /* Keys */
    GPIO_SetMode(PB, BIT15, GPIO_MODE_QUASI);
    GPIO_SetPullCtl(PB, BIT15, GPIO_PUSEL_PULL_UP);
    GPIO_SetMode(PC, BIT0, GPIO_MODE_QUASI);
    GPIO_SetPullCtl(PC, BIT0, GPIO_PUSEL_PULL_UP);
    GPIO_SET_DEBOUNCE_TIME(GPIO_DBCTL_DBCLKSRC_LIRC, GPIO_DBCTL_DBCLKSEL_512);
    GPIO_ENABLE_DEBOUNCE(PB, BIT15);
    GPIO_ENABLE_DEBOUNCE(PC, BIT0);

    /* Hall + IMU interrupt */
    GPIO_SetMode(PB, BIT7 | BIT8, GPIO_MODE_INPUT);
    GPIO_SetMode(PC, BIT5, GPIO_MODE_INPUT);

    /* Power/VBUS pins */
#if POWER_LOCK_ENABLE
    GPIO_SetMode(PA, BIT11, GPIO_MODE_OUTPUT);
#endif
    GPIO_SetMode(PA, BIT12, GPIO_MODE_INPUT);

    /* Safe default output state */
    bt_set_leds(0u, 0u);
    PC->DOUT &= ~BIT7;

    printf("[BT] GPIO init done (PB2/PB3/PB7/PB8/PB15/PC0/PC5/PC7/PA11/PA12)\n");
}

static void test_led_pair(BoardTestStats *stats)
{
    printf("[BT] Step 1/9 LED_GY: blink GREEN -> YELLOW -> BOTH\n");

    for (uint32_t i = 0u; i < 2u; i++)
    {
        bt_set_leds(0u, 1u);
        bt_delay(120u);
        bt_set_leds(0u, 0u);
        bt_delay(80u);
    }

    for (uint32_t i = 0u; i < 2u; i++)
    {
        bt_set_leds(1u, 0u);
        bt_delay(120u);
        bt_set_leds(0u, 0u);
        bt_delay(80u);
    }

    for (uint32_t i = 0u; i < 2u; i++)
    {
        bt_set_leds(1u, 1u);
        bt_delay(120u);
        bt_set_leds(0u, 0u);
        bt_delay(80u);
    }

    bt_record(stats, "LED_GY", BT_STATUS_MANUAL, "Verify PB3 green, PB2 yellow, and both-on states");
}

static void test_buzzer(BoardTestStats *stats)
{
    printf("[BT] Step 2/9 BUZZER_PC7: play two tones\n");

    BuzzerPlay(1200u, 120u);
    bt_delay(180u);
    BuzzerPlay(2000u, 120u);
    bt_delay(220u);

    bt_record(stats, "BUZZER_PC7", BT_STATUS_MANUAL, "Listen for two beeps with different pitch");
}

static void test_ws2812_matrix(BoardTestStats *stats)
{
#if USE_MOLE_GAME
    uint8_t ok = 1u;
    uint8_t saved_brightness;

    printf("[BT] Step 3/9 WS2812_PF6: RED -> GREEN -> BLUE -> RAINBOW\n");

    WS2812B_Init();
    saved_brightness = WS2812B_GetBrightness();
    WS2812B_SetBrightness(20u);

    WS2812B_FillRgb(255u, 0u, 0u);
    ok &= WS2812B_Refresh();
    bt_delay(BT_WS2812_STEP_MS);

    WS2812B_FillRgb(0u, 255u, 0u);
    ok &= WS2812B_Refresh();
    bt_delay(BT_WS2812_STEP_MS);

    WS2812B_FillRgb(0u, 0u, 255u);
    ok &= WS2812B_Refresh();
    bt_delay(BT_WS2812_STEP_MS);

    bt_fill_rainbow_by_16leds();
    ok &= WS2812B_Refresh();
    bt_delay(BT_WS2812_STEP_MS);

    WS2812B_Clear();
    ok &= WS2812B_Refresh();
    WS2812B_SetBrightness(saved_brightness);

    if (ok != 0u)
    {
        bt_record(stats, "WS2812_PF6", BT_STATUS_MANUAL, "Refresh OK; visually verify 16x16 RGB + rainbow pattern");
    }
    else
    {
        bt_record(stats, "WS2812_PF6", BT_STATUS_FAIL, "Driver refresh failed (check PF6/SPI0/PDMA/LED chain)");
    }
#else
    bt_record(stats, "WS2812_PF6", BT_STATUS_SKIP, "USE_MOLE_GAME disabled");
#endif
}

static void test_keya(BoardTestStats *stats)
{
    printf("[BT] Step 4/9 KEYA_PB15: press KEYA within %lu ms\n",
           (unsigned long)BT_KEY_TIMEOUT_MS);

    if (bt_wait_active_low(PB, BIT15, BT_KEY_TIMEOUT_MS) != 0u)
    {
        bt_record(stats, "KEYA_PB15", BT_STATUS_PASS, "Detected active-low press");
    }
    else
    {
        bt_record(stats, "KEYA_PB15", BT_STATUS_FAIL, "Timeout waiting for PB15 low");
    }
}

static void test_keyb(BoardTestStats *stats)
{
    printf("[BT] Step 5/9 KEYB_PC0: press KEYB within %lu ms\n",
           (unsigned long)BT_KEY_TIMEOUT_MS);

    if (bt_wait_active_low(PC, BIT0, BT_KEY_TIMEOUT_MS) != 0u)
    {
        bt_record(stats, "KEYB_PC0", BT_STATUS_PASS, "Detected active-low press");
    }
    else
    {
        bt_record(stats, "KEYB_PC0", BT_STATUS_FAIL, "Timeout waiting for PC0 low");
    }
}

#if USE_JUMP_DETECT
static void test_hall_inputs(BoardTestStats *stats)
{
    uint32_t start = get_ticks_ms();
    uint32_t pb7_prev = (PB->PIN & BIT7) ? 1u : 0u;
    uint32_t pb8_prev = (PB->PIN & BIT8) ? 1u : 0u;
    uint32_t pb7_edges = 0u;
    uint32_t pb8_edges = 0u;

    printf("[BT] Step 6/9 HALL_PB7_PB8: move magnet within %lu ms\n",
           (unsigned long)BT_HALL_TIMEOUT_MS);

    while (!is_timeout(start, BT_HALL_TIMEOUT_MS))
    {
        uint32_t pb7_now = (PB->PIN & BIT7) ? 1u : 0u;
        uint32_t pb8_now = (PB->PIN & BIT8) ? 1u : 0u;

        if (pb7_now != pb7_prev)
        {
            pb7_edges++;
            pb7_prev = pb7_now;
        }
        if (pb8_now != pb8_prev)
        {
            pb8_edges++;
            pb8_prev = pb8_now;
        }

        if ((pb7_edges + pb8_edges) > 0u)
        {
            printf("[BT] HALL snapshot PB7_edges=%lu PB8_edges=%lu\n",
                   (unsigned long)pb7_edges,
                   (unsigned long)pb8_edges);
            bt_record(stats, "HALL_PB7_PB8", BT_STATUS_PASS, "Detected Hall transition(s)");
            return;
        }

        bt_delay(BT_HALL_POLL_MS);
    }

    bt_record(stats, "HALL_PB7_PB8", BT_STATUS_FAIL, "No Hall transition detected");
}
#else
static void test_hall_inputs(BoardTestStats *stats)
{
    printf("[BT] Step 6/9 HALL_PB7_PB8: SKIPPED (jump detect disabled)\n");
    bt_record(stats, "HALL_PB7_PB8", BT_STATUS_SKIP, "Jump detection disabled (USE_JUMP_DETECT=0)");
}
#endif

static void test_power_pins(BoardTestStats *stats)
{
    uint8_t vbus;

    printf("[BT] Step 7/9 POWER: sample PA11 lock / PA12 VBUS\n");

    USBDetect_Init();
    vbus = USBDetect_IsHigh();

#if POWER_LOCK_ENABLE
    {
        uint32_t pa11 = (PA->PIN & BIT11) ? 1u : 0u;
        printf("[BT] POWER snapshot PA11=%lu VBUS=%u\n",
               (unsigned long)pa11,
               (unsigned)vbus);

        if (pa11 != 0u)
        {
            bt_record(stats, "POWER_PA11_PA12", BT_STATUS_PASS, "PA11 high; PA12(VBUS) snapshot printed above");
        }
        else
        {
            bt_record(stats, "POWER_PA11_PA12", BT_STATUS_FAIL, "PA11 low unexpectedly (power lock should stay asserted)");
        }
    }
#else
    printf("[BT] POWER snapshot PA11=DISABLED VBUS=%u\n", (unsigned)vbus);
    bt_record(stats, "POWER_PA11_PA12", BT_STATUS_SKIP, "PowerLock disabled by POWER_LOCK_ENABLE=0; VBUS sampled only");
#endif
}

static void test_vdda(BoardTestStats *stats)
{
    float vdda;
    uint32_t vdda_mv;

    printf("[BT] Step 8/9 VDDA_BG: begin power rail check\n");

    Adc_Init();
    Adc_UpdateVdda();
    vdda = Adc_GetVdda();
    vdda_mv = (uint32_t)(vdda * 1000.0f);

    printf("[BT] VDDA=%lu.%03luV\n",
           (unsigned long)(vdda_mv / 1000u),
           (unsigned long)(vdda_mv % 1000u));

    /* Include the exact measured voltage in the test record for clarity */
    {
        char detail[64];
        int len = snprintf(detail, sizeof(detail), "VDDA=%lu.%03luV",
                           (unsigned long)(vdda_mv / 1000u),
                           (unsigned long)(vdda_mv % 1000u));
        if (len < 0)
        {
            strncpy(detail, "VDDA snapshot (value unavailable)", sizeof(detail));
            detail[sizeof(detail) - 1] = '\0';
        }

        if ((vdda >= 2.0f) && (vdda <= 4.0f))
        {
            bt_record(stats, "VDDA_BG", BT_STATUS_PASS, detail);
        }
        else
        {
            bt_record(stats, "VDDA_BG", BT_STATUS_FAIL, detail);
        }
    }
}

static void test_imu_static(BoardTestStats *stats)
{
    uint32_t comm_fail = 0u;
    uint32_t valid = 0u;
    uint64_t gyro_abs_sum = 0u;
    float mean = 0.0f;
    float m2 = 0.0f;
    float stddev;
    uint32_t gyro_abs_mean;
    uint32_t pc5_int;
    uint8_t pass = 1u;
    const uint8_t is_sc7 = (GsensorGetDeviceType() == GSENSOR_DEVICE_SC7U22) ? 1u : 0u;

    printf("[BT] Step 9/9 IMU_I2C_PC5: keep board still for %lu samples\n",
           (unsigned long)BT_IMU_STATIC_SAMPLES);

    Gsensor_Init(GSENSOR_I2C_BUS_HZ, FSR_2G);
    GsensorWakeup();
    bt_delay(50u);

    for (uint32_t i = 0u; i < BT_IMU_STATIC_SAMPLES; i++)
    {
        int16_t acc[3] = {0};
        int16_t gyro[3] = {0};
        uint8_t ok = GsensorReadSixAxis(acc, gyro);

        if (ok == 0u)
        {
            comm_fail++;
            bt_delay(20u);
            continue;
        }

        {
            float mag_g = Gsensor_CalcMagnitude_g_from_raw(acc);
            float delta;

            valid++;
            delta = mag_g - mean;
            mean += delta / (float)valid;
            m2 += delta * (mag_g - mean);
        }

        gyro_abs_sum += (uint64_t)abs((int)gyro[0]);
        gyro_abs_sum += (uint64_t)abs((int)gyro[1]);
        gyro_abs_sum += (uint64_t)abs((int)gyro[2]);
        bt_delay(20u);
    }

    if (valid == 0u)
    {
        bt_record(stats, "IMU_I2C_PC5", BT_STATUS_FAIL, "No valid 6-axis sample");
        return;
    }

    stddev = (valid > 1u) ? sqrtf(m2 / (float)(valid - 1u)) : 0.0f;
    gyro_abs_mean = (uint32_t)(gyro_abs_sum / ((uint64_t)valid * 3ull));
    pc5_int = (PC->PIN & BIT5) ? 1u : 0u;

    printf("[BT] IMU snapshot sensor=%s INT_PC5=%lu mean=%.3f std=%.3f gyro_abs_mean=%lu comm_fail=%lu\n",
           GsensorGetDeviceName(),
           (unsigned long)pc5_int,
           (double)mean,
           (double)stddev,
           (unsigned long)gyro_abs_mean,
           (unsigned long)comm_fail);
    /* Build detailed reason string so test output explains why PASS/FAIL */
    {
        char detail[128];
        int len = 0;

        if (comm_fail > 0u)
        {
            pass = 0u;
        }
        if ((mean < BT_IMU_STATIC_ACC_MEAN_MIN_G) || (mean > BT_IMU_STATIC_ACC_MEAN_MAX_G))
        {
            pass = 0u;
        }
        if (stddev > BT_IMU_STATIC_ACC_STD_MAX_G)
        {
            pass = 0u;
        }
        if (is_sc7 && (gyro_abs_mean > BT_IMU_STATIC_GYRO_ABS_MEAN_MAX))
        {
            pass = 0u;
        }

        len = snprintf(detail, sizeof(detail), "sensor=%s INT=%lu mean=%.3f std=%.3f gyro_abs_mean=%lu comm_fail=%lu",
                       GsensorGetDeviceName(),
                       (unsigned long)pc5_int,
                       (double)mean,
                       (double)stddev,
                       (unsigned long)gyro_abs_mean,
                       (unsigned long)comm_fail);

        if (len < 0)
        {
            /* snprintf failed - fallback summary */
            strncpy(detail, "IMU snapshot (values not available)", sizeof(detail));
            detail[sizeof(detail) - 1] = '\0';
        }

        if (pass != 0u)
        {
            bt_record(stats, "IMU_I2C_PC5", BT_STATUS_PASS, detail);
        }
        else
        {
            bt_record(stats, "IMU_I2C_PC5", BT_STATUS_FAIL, detail);
        }
    }
}

static void bt_restore_outputs(void)
{
    bt_set_leds(0u, 0u);
    PC->DOUT &= ~BIT7;
#if USE_MOLE_GAME
    WS2812B_Clear();
    (void)WS2812B_Refresh();
#endif
}

void BoardTest_RunAll(void)
{
    BoardTestStats stats = {0};

    printf("\n=== RL_SPORT V3 Board Test ===\n");
    printf("[BT] FLOW=FULL IO={PB2:Y_LED,PB3:G_LED,PB15:KEYA,PC0:KEYB,PB7/PB8:HALL,PC7:BUZZER,PC5:IMU_INT,PF6:WS2812,PA12:VBUS}\n");
    printf("[BT] Start tests...\n");

    BoardTest_GPIO_Init();
    test_led_pair(&stats);
    bt_delay(20u);
    test_buzzer(&stats);
    bt_delay(20u);
    test_ws2812_matrix(&stats);
    bt_delay(20u);
    test_keya(&stats);
    bt_delay(20u);
    test_keyb(&stats);
    bt_delay(20u);
    test_hall_inputs(&stats);
    bt_delay(20u);
    test_power_pins(&stats);
    bt_delay(20u);
    test_vdda(&stats);
    bt_delay(20u);
    test_imu_static(&stats);

    bt_restore_outputs();

    printf("[BT] SUMMARY: PASS=%lu FAIL=%lu MANUAL=%lu SKIP=%lu\n",
           (unsigned long)stats.pass_count,
           (unsigned long)stats.fail_count,
           (unsigned long)stats.manual_count,
           (unsigned long)stats.skip_count);
    printf("[BT] End.\n");
}
