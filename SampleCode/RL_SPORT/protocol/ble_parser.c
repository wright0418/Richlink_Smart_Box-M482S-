#include "ble_parser.h"

#include <string.h>

static const struct
{
    BleCmdType type;
    const char *keyword;
} s_ble_cmd_table[] = {
    {BLE_CMD_CONNECTED, ": CONNECTED OK"},
    {BLE_CMD_DISCONNECTED, ": DISCONNECTED OK"},
    {BLE_CMD_CMD_MODE, ": CMD_MODE OK"},
    {BLE_CMD_DATA_MODE, ": DATA_MODE OK"},
    {BLE_CMD_CONN_START, "conn st"},
    {BLE_CMD_GET_CYCLE, "get cycle"},
    {BLE_CMD_SET_END, "set end"},
    {BLE_CMD_DISC_MSG, "disc"},
    {BLE_CMD_MAC_ADDR, "MAC_ADDR"},
    {BLE_CMD_MAC_ADDR, "ADDR"},
    {BLE_CMD_DEVICE_NAME, "DEVICE_NAME"},
    {BLE_CMD_DEVICE_NAME, "NAME"}};

static uint8_t BleParser_IsHexChar(char c)
{
    return (uint8_t)((c >= '0' && c <= '9') ||
                     (c >= 'A' && c <= 'F') ||
                     (c >= 'a' && c <= 'f'));
}

static char BleParser_ToUpperHex(char c)
{
    if (c >= 'a' && c <= 'f')
    {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

BleCmdType BleParser_ParseCommand(const char *msg)
{
    if (!msg)
    {
        return BLE_CMD_NONE;
    }

    for (uint32_t i = 0u; i < (uint32_t)(sizeof(s_ble_cmd_table) / sizeof(s_ble_cmd_table[0])); ++i)
    {
        if (strstr(msg, s_ble_cmd_table[i].keyword) != NULL)
        {
            return s_ble_cmd_table[i].type;
        }
    }

    return BLE_CMD_NONE;
}

void BleParser_StripCmdModeMarker(char *msg, const char *marker)
{
    if (!msg || !marker)
    {
        return;
    }

    const size_t marker_len = strlen(marker);
    if (marker_len == 0u)
    {
        return;
    }

    char *p = NULL;
    while ((p = strstr(msg, marker)) != NULL)
    {
        size_t tail_len = strlen(p + marker_len);
        memmove(p, p + marker_len, tail_len + 1u);
    }
}

uint8_t BleParser_ExtractMacSuffix4(const char *src, char *out4)
{
    if (!src || !out4)
    {
        return 0u;
    }

    char rev_hex4[4];
    uint8_t found = 0u;
    size_t len = strlen(src);

    while (len > 0u && found < 4u)
    {
        char c = src[len - 1u];
        if (BleParser_IsHexChar(c))
        {
            rev_hex4[found++] = BleParser_ToUpperHex(c);
        }
        len--;
    }

    if (found < 4u)
    {
        out4[0] = '\0';
        return 0u;
    }

    out4[0] = rev_hex4[3];
    out4[1] = rev_hex4[2];
    out4[2] = rev_hex4[1];
    out4[3] = rev_hex4[0];
    out4[4] = '\0';
    return 1u;
}

uint8_t BleParser_ExtractRopeSuffix4(const char *name, char *out4)
{
    if (!name || !out4)
    {
        return 0u;
    }

    const char *p = strstr(name, "ROPE_");
    if (!p)
    {
        out4[0] = '\0';
        return 0u;
    }

    p += 5;
    for (uint8_t i = 0u; i < 4u; i++)
    {
        if (!BleParser_IsHexChar(p[i]))
        {
            out4[0] = '\0';
            return 0u;
        }
        out4[i] = BleParser_ToUpperHex(p[i]);
    }

    out4[4] = '\0';
    return 1u;
}

uint8_t BleParser_IsNameQueryEcho(const char *s)
{
    if (!s)
    {
        return 0u;
    }
    return (uint8_t)(strstr(s, "AT+NAME=?") != NULL);
}

uint8_t BleParser_IsAddrQueryEcho(const char *s)
{
    if (!s)
    {
        return 0u;
    }
    return (uint8_t)(strstr(s, "AT+ADDR=?") != NULL);
}