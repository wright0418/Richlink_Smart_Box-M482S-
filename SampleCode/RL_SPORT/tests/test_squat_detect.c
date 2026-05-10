#include <stdio.h>
#include <stdint.h>

#include "../app/algorithms/squat_detect.h"

static int s_failed = 0;

#define EXPECT_EQ_U16(actual, expected, name)                     \
    do                                                            \
    {                                                             \
        uint16_t _a = (uint16_t)(actual);                         \
        uint16_t _e = (uint16_t)(expected);                       \
        if (_a != _e)                                             \
        {                                                         \
            s_failed++;                                           \
            printf("[FAIL] %s: actual=%u expected=%u\\n", (name), \
                   (unsigned)_a, (unsigned)_e);                   \
        }                                                         \
        else                                                      \
        {                                                         \
            printf("[PASS] %s\\n", (name));                       \
        }                                                         \
    } while (0)

#define EXPECT_EQ_U8(actual, expected, name)                      \
    do                                                            \
    {                                                             \
        uint8_t _a = (uint8_t)(actual);                           \
        uint8_t _e = (uint8_t)(expected);                         \
        if (_a != _e)                                             \
        {                                                         \
            s_failed++;                                           \
            printf("[FAIL] %s: actual=%u expected=%u\\n", (name), \
                   (unsigned)_a, (unsigned)_e);                   \
        }                                                         \
        else                                                      \
        {                                                         \
            printf("[PASS] %s\\n", (name));                       \
        }                                                         \
    } while (0)

static void feed_const(int16_t ax,
                       int16_t ay,
                       int16_t az,
                       uint32_t samples,
                       uint32_t dt_ms,
                       uint32_t *now_ms,
                       uint16_t *rep_count)
{
    for (uint32_t i = 0u; i < samples; i++)
    {
        if (SquatDetect_ProcessSample(ax, ay, az, *now_ms) != 0u)
        {
            (*rep_count)++;
        }
        *now_ms += dt_ms;
    }
}

static void test_single_valid_rep(void)
{
    uint32_t now_ms = 0u;
    uint16_t reps = 0u;

    SquatDetect_Init();
    SquatDetect_Reset();

    /* 50Hz sequence: stand -> descend -> bottom hold -> ascend -> stand */
    feed_const(0, 0, 1024, 30u, 20u, &now_ms, &reps); /* settle stand */
    feed_const(0, 0, 200, 14u, 20u, &now_ms, &reps);  /* strong descend */
    feed_const(0, 0, 1024, 22u, 20u, &now_ms, &reps); /* bottom hold-ish */
    feed_const(0, 0, 1900, 10u, 20u, &now_ms, &reps); /* ascend impulse */
    feed_const(0, 0, 1024, 20u, 20u, &now_ms, &reps); /* stand stabilize */

    EXPECT_EQ_U16(reps, 1u, "squat: valid motion counted once");
    EXPECT_EQ_U16(SquatDetect_GetCount(), 1u, "squat: count API reports one rep");
    EXPECT_EQ_U8(SquatDetect_GetPhase(), SQUAT_PHASE_STAND, "squat: phase returns to STAND");
}

static void test_noise_not_counted(void)
{
    uint32_t now_ms = 0u;
    uint16_t reps = 0u;

    SquatDetect_Init();
    SquatDetect_Reset();

    for (uint32_t i = 0u; i < 220u; i++)
    {
        int16_t z = (i & 1u) ? 1080 : 960;
        if (SquatDetect_ProcessSample(0, 0, z, now_ms) != 0u)
        {
            reps++;
        }
        now_ms += 20u;
    }

    EXPECT_EQ_U16(reps, 0u, "squat: stationary jitter not counted");
    EXPECT_EQ_U16(SquatDetect_GetCount(), 0u, "squat: count remains zero for jitter");
}

int main(void)
{
    printf("Running squat_detect unit tests...\\n");

    test_single_valid_rep();
    test_noise_not_counted();

    if (s_failed == 0)
    {
        printf("\\nAll tests passed.\\n");
        return 0;
    }

    printf("\\nTests failed: %d\\n", s_failed);
    return 1;
}
