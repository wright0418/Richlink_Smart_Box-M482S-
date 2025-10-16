#include "mesh_handler.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// 定義 NULL 為 (void*)0 以支援 C99
#define NULL ((void *)0)

// 簡單的 string 函數實作（避免依賴 string.h）
static char *my_strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;

    for (; *haystack; haystack++)
    {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }
        if (!*n)
            return (char *)haystack;
    }
    return (char *)0;
}

static char *my_strtok(char *str, const char *delim)
{
    static char *saved = (char *)0;
    if (str)
        saved = str;
    if (!saved)
        return (char *)0;

    // 跳過前導分隔符
    while (*saved && *saved == delim[0])
        saved++;
    if (!*saved)
        return (char *)0;

    char *start = saved;
    // 找到下一個分隔符
    while (*saved && *saved != delim[0])
        saved++;
    if (*saved)
        *saved++ = '\0';

    return start;
}

// Mesh 處理狀態全域變數
static mesh_handler_state_t g_mesh_state = {0};
static mesh_handler_callbacks_t g_mesh_callbacks = {0};

// 小工具：十六進位字串轉 bytes（僅接受 [0-9A-Fa-f]，長度需為偶數）
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return 10 + (int)(c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (int)(c - 'A');
    return -1;
}

static uint32_t hex_to_bytes(const char *hex, uint8_t *out, uint32_t max_out)
{
    // 跳過前導空白
    while (*hex == ' ' || *hex == '\t')
        hex++;

    // 找到有效hex字串的結尾
    const char *p = hex;
    uint32_t n = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
    {
        if (hex_nibble(*p) < 0)
            return 0; // 非 hex 字元
        n++;
        p++;
    }

    // 檢查長度：必須>0且為偶數
    if (n == 0 || (n & 1u) != 0u)
        return 0;

    uint32_t out_len = n / 2u;
    if (out_len > max_out)
        out_len = max_out;

    // 轉換hex到bytes
    for (uint32_t i = 0; i < out_len; i++)
    {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return out_len;
}

void mesh_handler_init(const mesh_handler_callbacks_t *callbacks)
{
    // 初始化狀態
    g_mesh_state.is_bound = false;
    g_mesh_state.device_uid[0] = '\0';
    g_mesh_state.last_sender_uid[0] = '\0';
    g_mesh_state.last_payload_len = 0;
    g_mesh_state.msg_count = 0;

    // 複製回調函數
    if (callbacks != (void *)0)
    {
        g_mesh_callbacks = *callbacks;
    }
}

void mesh_handler_event(ble_mesh_at_event_t event, const char *data)
{
    switch (event)
    {
    case BLE_MESH_AT_EVENT_VER_SUCCESS:
        if (g_mesh_callbacks.led_yellow != (void *)0)
        {
            g_mesh_callbacks.led_yellow(true);
        }
        break;

    case BLE_MESH_AT_EVENT_REBOOT_SUCCESS:
        // 收到 REBOOT-MSG SUCCESS，藍燈短閃
        if (g_mesh_callbacks.led_pulse_blue != NULL)
        {
            g_mesh_callbacks.led_pulse_blue(120);
        }
        break;

    case BLE_MESH_AT_EVENT_PROV_BOUND:
        // 綁定成功
        g_mesh_state.is_bound = true;

        // 記錄 UID
        if (data != NULL)
        {
            size_t k = 0;
            while (data[k] && k < sizeof(g_mesh_state.device_uid) - 1)
            {
                g_mesh_state.device_uid[k] = data[k];
                k++;
            }
            g_mesh_state.device_uid[k] = '\0';
        }

        // 通知 LED 綁定狀態
        if (g_mesh_callbacks.led_binding != NULL)
        {
            g_mesh_callbacks.led_binding(true);
        }
        break;

    case BLE_MESH_AT_EVENT_PROV_UNBOUND:
        // 未綁定/解除綁定
        g_mesh_state.is_bound = false;
        g_mesh_state.device_uid[0] = '\0';

        // 通知 LED 綁定狀態
        if (g_mesh_callbacks.led_binding != NULL)
        {
            g_mesh_callbacks.led_binding(false);
        }
        break;

    case BLE_MESH_AT_EVENT_LINE_RECEIVED:
        // 藍燈短脈衝表示有收到行
        if (g_mesh_callbacks.led_pulse_blue != NULL)
        {
            g_mesh_callbacks.led_pulse_blue(120);
        }
        // 嘗試解析 MDTSG/MDTPG
        if (data != NULL)
        {
            mesh_handler_process_line(data);
        }
        break;

    case BLE_MESH_AT_EVENT_TIMEOUT:
        // 紅燈長脈衝表示逾時
        if (g_mesh_callbacks.led_pulse_red != NULL)
        {
            g_mesh_callbacks.led_pulse_red(500);
        }
        break;

    case BLE_MESH_AT_EVENT_ERROR:
        // 紅燈長脈衝表示錯誤
        if (g_mesh_callbacks.led_pulse_red != NULL)
        {
            g_mesh_callbacks.led_pulse_red(500);
        }
        break;

    default:
        break;
    }
}

void mesh_handler_process_line(const char *line)
{
    // 複製到本地緩衝以便分詞
    char buf[128];
    uint32_t i = 0;
    while (line[i] && i < sizeof(buf) - 1)
    {
        buf[i] = line[i];
        i++;
    }
    buf[i] = '\0';

    // 檢查是否 MDTSG-MSG、MDTPG-MSG 或 MDTS-MSG
    const char *key1 = "MDTSG-MSG";
    const char *key2 = "MDTPG-MSG";
    const char *key3 = "MDTS-MSG";
    if (my_strstr(buf, key1) == (char *)0 &&
        my_strstr(buf, key2) == (char *)0 &&
        my_strstr(buf, key3) == (char *)0)
        return;

    // 以空白切分，取 tokens[1] 為 sender，最後一個 token 當作 hex payload
    char *tokens[8];
    int tcount = 0;
    char *tok = my_strtok(buf, " ");
    while (tok && tcount < 8)
    {
        tokens[tcount++] = tok;
        tok = my_strtok((char *)0, " ");
    }
    if (tcount < 3)
        return;

    const char *sender = tokens[1];
    const char *hex;
    if (my_strstr(tokens[0], key3) != (char *)0 && tcount >= 4)
    {
        hex = tokens[3];
    }
    else
    {
        hex = tokens[tcount - 1];
    }

    // 儲存 sender
    uint32_t sl = 0;
    while (sender[sl] && sl < sizeof(g_mesh_state.last_sender_uid) - 1)
    {
        g_mesh_state.last_sender_uid[sl] = sender[sl];
        sl++;
    }
    g_mesh_state.last_sender_uid[sl] = '\0';

    // 轉換 hex 到 bytes
    g_mesh_state.last_payload_len = hex_to_bytes(hex, g_mesh_state.last_payload, (uint32_t)sizeof(g_mesh_state.last_payload));
    g_mesh_state.msg_count++;

    // 根據訊息類型觸發黃燈閃爍
    if (my_strstr(tokens[0], key1) != (char *)0 && g_mesh_callbacks.led_flash != (void *)0)
        g_mesh_callbacks.led_flash(1); // MDTSG 黃燈閃 1 次
    else if (my_strstr(tokens[0], key2) != (char *)0 && g_mesh_callbacks.led_flash != (void *)0)
        g_mesh_callbacks.led_flash(2); // MDTPG 黃燈閃 2 次
    else if (my_strstr(tokens[0], key3) != (char *)0 && g_mesh_callbacks.led_flash != (void *)0)
        g_mesh_callbacks.led_flash(3); // MDTS 黃燈閃 3 次

    // 檢查是否為 MESH MODBUS Agent 格式
    // RL Mode: 0x82 0x76 開頭
    // Bypass Mode: 0x01-0xFF 開頭（8 bytes 以上）
    bool is_agent_message = false;
    if (g_mesh_state.last_payload_len >= 8)
    {
        // 檢查 RL Mode (header = 0x82 0x76)
        if (g_mesh_state.last_payload_len >= 11 &&
            g_mesh_state.last_payload[0] == 0x82 &&
            g_mesh_state.last_payload[1] == 0x76)
        {
            is_agent_message = true;
        }
        // 檢查 Bypass Mode (標準 MODBUS RTU 格式，8 bytes)
        else if (g_mesh_state.last_payload_len == 8)
        {
            // 基本檢查：第一個位元組是 slave address (1-247)
            // 第二個位元組是 function code (常見：0x03, 0x04, 0x06, 0x10)
            uint8_t slave_addr = g_mesh_state.last_payload[0];
            uint8_t func_code = g_mesh_state.last_payload[1];
            if (slave_addr >= 1 && slave_addr <= 247 &&
                (func_code == 0x03 || func_code == 0x04 || func_code == 0x06 || func_code == 0x10))
            {
                is_agent_message = true;
            }
        }
    }

    // 如果是 Agent 訊息，轉發給 Agent 回調處理
    if (is_agent_message && g_mesh_callbacks.agent_response != (void *)0)
    {
        g_mesh_callbacks.agent_response(g_mesh_state.last_payload, (uint8_t)g_mesh_state.last_payload_len);
        return; // Agent 訊息不執行後續的 PA6 控制邏輯
    }

    // 解析有效負載：若為單一位元組，0x30=OFF、0x31=ON
    if (g_mesh_state.last_payload_len == 1 && g_mesh_callbacks.pa6_control != (void *)0)
    {
        uint8_t b = g_mesh_state.last_payload[0];
        if (b == 0x30u)
        {
            // OFF 命令
            g_mesh_callbacks.pa6_control(false);
        }
        else if (b == 0x31u)
        {
            // ON 命令
            g_mesh_callbacks.pa6_control(true);
        }
    }
}

const mesh_handler_state_t *mesh_handler_get_state(void)
{
    return &g_mesh_state;
}

bool mesh_handler_is_bound(void)
{
    return g_mesh_state.is_bound;
}

const char *mesh_handler_get_device_uid(void)
{
    return g_mesh_state.device_uid;
}