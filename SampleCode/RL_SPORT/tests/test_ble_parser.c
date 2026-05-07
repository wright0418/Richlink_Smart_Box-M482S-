#include <stdio.h>
#include <string.h>

#include "../protocol/ble_parser.h"

static int s_failed = 0;

#define EXPECT_TRUE(cond, name)            \
    do                                     \
    {                                      \
        if (!(cond))                       \
        {                                  \
            s_failed++;                    \
            printf("[FAIL] %s\n", (name)); \
        }                                  \
        else                               \
        {                                  \
            printf("[PASS] %s\n", (name)); \
        }                                  \
    } while (0)

#define EXPECT_EQ_INT(actual, expected, name)            \
    do                                                   \
    {                                                    \
        int _a = (int)(actual);                          \
        int _e = (int)(expected);                        \
        if (_a != _e)                                    \
        {                                                \
            s_failed++;                                  \
            printf("[FAIL] %s: actual=%d expected=%d\n", \
                   (name), _a, _e);                      \
        }                                                \
        else                                             \
        {                                                \
            printf("[PASS] %s\n", (name));               \
        }                                                \
    } while (0)

#define EXPECT_STR_EQ(actual, expected, name)            \
    do                                                   \
    {                                                    \
        const char *_a = (actual);                       \
        const char *_e = (expected);                     \
        if (strcmp(_a, _e) != 0)                         \
        {                                                \
            s_failed++;                                  \
            printf("[FAIL] %s: actual=%s expected=%s\n", \
                   (name), _a, _e);                      \
        }                                                \
        else                                             \
        {                                                \
            printf("[PASS] %s\n", (name));               \
        }                                                \
    } while (0)

static void test_parse_command(void)
{
    EXPECT_EQ_INT(BleParser_ParseCommand(": CONNECTED OK"), BLE_CMD_CONNECTED,
                  "parse: connected");
    EXPECT_EQ_INT(BleParser_ParseCommand(": DISCONNECTED OK"), BLE_CMD_DISCONNECTED,
                  "parse: disconnected");
    EXPECT_EQ_INT(BleParser_ParseCommand(": CMD_MODE OK"), BLE_CMD_CMD_MODE,
                  "parse: cmd mode");
    EXPECT_EQ_INT(BleParser_ParseCommand("get cycle"), BLE_CMD_GET_CYCLE,
                  "parse: get cycle");
    EXPECT_EQ_INT(BleParser_ParseCommand("set end"), BLE_CMD_SET_END,
                  "parse: set end");
    EXPECT_EQ_INT(BleParser_ParseCommand("DEVICE_NAME: ROPE_1A2B"), BLE_CMD_DEVICE_NAME,
                  "parse: device name");
    EXPECT_EQ_INT(BleParser_ParseCommand("12:34:56:78:9A:BC"), BLE_CMD_MAC_ADDR,
                  "parse: plain colon mac");
    EXPECT_EQ_INT(BleParser_ParseCommand("A1-B2-C3-D4-E5-F6"), BLE_CMD_MAC_ADDR,
                  "parse: plain dash mac");
    EXPECT_EQ_INT(BleParser_ParseCommand("001122AABBCC"), BLE_CMD_MAC_ADDR,
                  "parse: plain contiguous mac");
    EXPECT_EQ_INT(BleParser_ParseCommand("unknown response"), BLE_CMD_NONE,
                  "parse: unknown");
}

static void test_strip_cmd_marker(void)
{
    char msg1[64] = "!CCMD@: CONNECTED OK";
    char msg2[64] = "foo!CCMD@bar!CCMD@baz";
    char msg3[64] = "plain text";

    BleParser_StripCmdModeMarker(msg1, "!CCMD@");
    BleParser_StripCmdModeMarker(msg2, "!CCMD@");
    BleParser_StripCmdModeMarker(msg3, "!CCMD@");

    EXPECT_STR_EQ(msg1, ": CONNECTED OK", "strip: leading marker");
    EXPECT_STR_EQ(msg2, "foobarbaz", "strip: repeated marker");
    EXPECT_STR_EQ(msg3, "plain text", "strip: unchanged");
}

static void test_extract_mac_suffix(void)
{
    char out[5];

    EXPECT_TRUE(BleParser_ExtractMacSuffix4("MAC_ADDR: 12:34:56:78:9a:bc", out) == 1u,
                "mac suffix: parse success");
    EXPECT_STR_EQ(out, "9ABC", "mac suffix: uppercase rightmost 4");

    EXPECT_TRUE(BleParser_ExtractMacSuffix4("ADDR=AA-BB-CC-DD", out) == 1u,
                "mac suffix: alternate format");
    EXPECT_STR_EQ(out, "CCDD", "mac suffix: alternate rightmost 4");

    EXPECT_TRUE(BleParser_ExtractMacSuffix4("ADDR=?", out) == 0u,
                "mac suffix: fail on insufficient hex");
}

static void test_extract_rope_suffix(void)
{
    char out[5];

    EXPECT_TRUE(BleParser_ExtractRopeSuffix4("DEVICE_NAME: ROPE_ab12", out) == 1u,
                "rope suffix: parse success");
    EXPECT_STR_EQ(out, "AB12", "rope suffix: uppercase");

    EXPECT_TRUE(BleParser_ExtractRopeSuffix4("DEVICE_NAME: ROPE_12G4", out) == 0u,
                "rope suffix: reject non-hex");
    EXPECT_TRUE(BleParser_ExtractRopeSuffix4("DEVICE_NAME: ROPR_1234", out) == 0u,
                "rope suffix: reject missing prefix");
}

static void test_echo_detection(void)
{
    EXPECT_TRUE(BleParser_IsNameQueryEcho("AT+NAME=?") == 1u,
                "echo: name query");
    EXPECT_TRUE(BleParser_IsNameQueryEcho("DEVICE_NAME: ROPE_1234") == 0u,
                "echo: name real response");
    EXPECT_TRUE(BleParser_IsAddrQueryEcho("AT+ADDR=?") == 1u,
                "echo: addr query");
    EXPECT_TRUE(BleParser_IsAddrQueryEcho("MAC_ADDR: 01:02:03:04:05:06") == 0u,
                "echo: addr real response");
}

int main(void)
{
    printf("Running RL_SPORT BLE parser unit tests...\n");

    test_parse_command();
    test_strip_cmd_marker();
    test_extract_mac_suffix();
    test_extract_rope_suffix();
    test_echo_detection();

    if (s_failed == 0)
    {
        printf("\nAll parser tests passed.\n");
        return 0;
    }

    printf("\nParser tests failed: %d\n", s_failed);
    return 1;
}