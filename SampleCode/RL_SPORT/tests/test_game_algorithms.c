#include <stdio.h>
#include <stdint.h>
#include "../game_logic.h"

static int s_failed = 0;

#define EXPECT_EQ_U8(actual, expected, name)                     \
    do                                                           \
    {                                                            \
        uint8_t _a = (uint8_t)(actual);                          \
        uint8_t _e = (uint8_t)(expected);                        \
        if (_a != _e)                                            \
        {                                                        \
            s_failed++;                                          \
            printf("[FAIL] %s: actual=%u expected=%u\n", (name), \
                   (unsigned)_a, (unsigned)_e);                  \
        }                                                        \
        else                                                     \
        {                                                        \
            printf("[PASS] %s\n", (name));                       \
        }                                                        \
    } while (0)

#define EXPECT_EQ_U16(actual, expected, name)                    \
    do                                                           \
    {                                                            \
        uint16_t _a = (uint16_t)(actual);                        \
        uint16_t _e = (uint16_t)(expected);                      \
        if (_a != _e)                                            \
        {                                                        \
            s_failed++;                                          \
            printf("[FAIL] %s: actual=%u expected=%u\n", (name), \
                   (unsigned)_a, (unsigned)_e);                  \
        }                                                        \
        else                                                     \
        {                                                        \
            printf("[PASS] %s\n", (name));                       \
        }                                                        \
    } while (0)

static void test_movement_algo(void)
{
    EXPECT_EQ_U8(GameAlgo_IsMovement(0.03f, 1.00f, 0.02f, 0.40f), 1u,
                 "movement: stddev above threshold");
    EXPECT_EQ_U8(GameAlgo_IsMovement(0.01f, 1.60f, 0.02f, 0.40f), 1u,
                 "movement: mean magnitude deviation above tolerance");
    EXPECT_EQ_U8(GameAlgo_IsMovement(0.01f, 1.20f, 0.02f, 0.40f), 0u,
                 "movement: within thresholds => stationary");
    EXPECT_EQ_U8(GameAlgo_IsMovement(0.02f, 1.40f, 0.02f, 0.40f), 0u,
                 "movement: exact threshold boundary uses strict greater-than");
}

static void test_jump_algo(void)
{
    uint8_t residual = 0u;

    EXPECT_EQ_U16(GameAlgo_CalcJumpsFromEdges(0u, 0u, &residual), 0u,
                  "jump: no edges");
    EXPECT_EQ_U8(residual, 0u, "jump: residual remains 0");

    EXPECT_EQ_U16(GameAlgo_CalcJumpsFromEdges(0u, 1u, &residual), 0u,
                  "jump: one edge no full jump");
    EXPECT_EQ_U8(residual, 1u, "jump: residual becomes 1");

    EXPECT_EQ_U16(GameAlgo_CalcJumpsFromEdges(residual, 1u, &residual), 1u,
                  "jump: residual+one edge => one jump");
    EXPECT_EQ_U8(residual, 0u, "jump: residual back to 0");

    EXPECT_EQ_U16(GameAlgo_CalcJumpsFromEdges(1u, 2u, &residual), 1u,
                  "jump: three edges => one jump + residual one edge");
    EXPECT_EQ_U8(residual, 1u, "jump: residual one edge");

    EXPECT_EQ_U16(GameAlgo_CalcJumpsFromEdges(0u, 2u, NULL), 0u,
                  "jump: null residual pointer => safe no-op");
}

int main(void)
{
    printf("Running RL_SPORT algorithm unit tests...\n");
    test_movement_algo();
    test_jump_algo();

    if (s_failed == 0)
    {
        printf("\nAll tests passed.\n");
        return 0;
    }

    printf("\nTests failed: %d\n", s_failed);
    return 1;
}
