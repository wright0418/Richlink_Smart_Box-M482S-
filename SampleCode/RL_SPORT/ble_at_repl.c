#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(__has_include)
#if __has_include("NuMicro.h")
#include "NuMicro.h"
#else
#include "host_compat.h"
#endif
#else
#include "NuMicro.h"
#endif

#include "project_config.h"
#include "ble_at_repl.h"
#include "system_status.h"
#include "timer.h"
#include "led.h"
#include "buzzer.h"
#include "gsensor.h"
#include "adc.h"
#include "ble.h"
#include "fmc.h"

#if USE_BLE_AT_REPL

#define REPL_PREFIX "AT+TEST,"
#define REPL_STREAM_MIN_INTERVAL_MS 50u
#define REPL_STREAM_MAX_INTERVAL_MS 2000u

/* ---------- Firmware version (compile-time) ---------- */
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0

/* ---------- Data Flash layout ---------- */
/* The last page (4 KB) of APROM is reserved as Data Flash for user parameters.
 * Adjust DFLASH_BASE if Config0 DFBA is set differently.                     */
#define DFLASH_BASE 0x0007F000UL /* last 4 KB page of 512 KB APROM */
#define DFLASH_SIZE 0x1000UL     /* 4 KB = 1 page                  */
#define DFLASH_MAX_RW_LEN 64u    /* max words per single R/W cmd   */

/* ---------- Unified error codes ---------- */
#define ERR_STATE "STATE" /* command not allowed in current state     */
#define ERR_PARAM "PARAM" /* invalid / missing parameter              */
#define ERR_CMD "CMD"     /* unknown command                          */
#define ERR_BUSY "BUSY"   /* resource busy (e.g. stream running)      */
#define ERR_FLASH "FLASH" /* Data-Flash operation failed              */
#define ERR_RANGE "RANGE" /* address / length out of range            */
#define ERR_ALIGN "ALIGN" /* address not 4-byte aligned               */

typedef struct
{
    uint8_t stream_source;
    uint8_t stream_enabled;
    uint32_t stream_interval_ms;
    uint32_t last_stream_ms;
} BleAtReplCtx;

typedef enum
{
    REPL_STREAM_SRC_GSENSOR = 0,
    REPL_STREAM_SRC_ADC,
    REPL_STREAM_SRC_HALL,
    REPL_STREAM_SRC_KEY
} ReplStreamSource;

static BleAtReplCtx s_repl;

static int repl_send(const char *fmt, ...)
{
    /* Increase local buffer to avoid truncation for long HELP / INFO replies. */
    char out[512];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(out, sizeof(out), fmt, ap);
    va_end(ap);
    if (len < 0)
    {
        return len;
    }
    return BLE_UART_SEND((void *)UART1, "%s", out);
}

static void repl_send_ok(const char *cmd, const char *payload)
{
    if (payload && payload[0] != '\0')
    {
        repl_send("+OK,%s,%s\r\n", cmd, payload);
    }
    else
    {
        repl_send("+OK,%s\r\n", cmd);
    }
}

static void repl_send_err(const char *code, const char *msg)
{
    repl_send("+ERR,%s,%s\r\n", code, msg);
}

static const char *skip_prefix(const char *msg)
{
    size_t plen = strlen(REPL_PREFIX);
    if (!msg)
    {
        return NULL;
    }
    if (strncmp(msg, REPL_PREFIX, plen) == 0)
    {
        return msg + plen;
    }
    return NULL;
}

static uint32_t clamp_stream_interval(uint32_t req)
{
    if (req < REPL_STREAM_MIN_INTERVAL_MS)
    {
        return REPL_STREAM_MIN_INTERVAL_MS;
    }
    if (req > REPL_STREAM_MAX_INTERVAL_MS)
    {
        return REPL_STREAM_MAX_INTERVAL_MS;
    }
    return req;
}

static uint8_t parse_stream_source(const char *token, uint8_t *out_source)
{
    if (!token || !out_source)
    {
        return 0u;
    }

    if (strcmp(token, "GSENSOR") == 0 || strcmp(token, "SENSOR") == 0)
    {
        *out_source = (uint8_t)REPL_STREAM_SRC_GSENSOR;
        return 1u;
    }
    if (strcmp(token, "ADC") == 0)
    {
        *out_source = (uint8_t)REPL_STREAM_SRC_ADC;
        return 1u;
    }
    if (strcmp(token, "HALL") == 0)
    {
        *out_source = (uint8_t)REPL_STREAM_SRC_HALL;
        return 1u;
    }
    if (strcmp(token, "KEY") == 0)
    {
        *out_source = (uint8_t)REPL_STREAM_SRC_KEY;
        return 1u;
    }
    return 0u;
}

static const char *stream_source_name(uint8_t source)
{
    switch (source)
    {
    case REPL_STREAM_SRC_ADC:
        return "ADC";
    case REPL_STREAM_SRC_HALL:
        return "HALL";
    case REPL_STREAM_SRC_KEY:
        return "KEY";
    case REPL_STREAM_SRC_GSENSOR:
    default:
        return "GSENSOR";
    }
}

static uint8_t is_decimal_number(const char *s)
{
    size_t i;
    if (!s || s[0] == '\0')
    {
        return 0u;
    }
    for (i = 0u; s[i] != '\0'; i++)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            return 0u;
        }
    }
    return 1u;
}

static void trim_line_end(char *s)
{
    size_t len;
    if (!s)
    {
        return;
    }

    len = strlen(s);
    while (len > 0u)
    {
        char c = s[len - 1u];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
        {
            s[len - 1u] = '\0';
            len--;
        }
        else
        {
            break;
        }
    }
}

void BleAtRepl_Init(void)
{
    memset(&s_repl, 0, sizeof(s_repl));
    s_repl.stream_source = (uint8_t)REPL_STREAM_SRC_GSENSOR;
    s_repl.stream_interval_ms = 200u;
    Sys_SetReplMode(0u);
}

uint8_t BleAtRepl_IsActive(void)
{
    return Sys_GetReplMode();
}

static void repl_activate(void)
{
    Sys_SetReplMode(1u);
    Sys_SetGameState(GAME_STOP);
    Sys_SetIdleState(0u);
    s_repl.last_stream_ms = get_ticks_ms();
    /* When entering REPL, stop the main blink engine so REPL LED commands
       (LED_ON / LED_OFF / LED_BLINK) take immediate and persistent effect. */
    SetGreenLedMode(0, 0);
    SetGreenLed(0);
    DBG_PRINT("[REPL] Activated - disabled main LED blink and cleared LED\n");
}

static void handle_repl_start(void)
{
    repl_activate();
    repl_send_ok("REPL_START", "READY");
}

static void handle_repl_stop(void)
{
    s_repl.stream_enabled = 0u;
    Sys_SetReplMode(0u);
    /* Clear any LED override when exiting REPL to let main resume control */
    Sys_SetLedOverride(0u);
    repl_send_ok("REPL_STOP", "BYE");
}

static void handle_led_cmd(const char *args)
{
    if (strcmp(args, "LED_ON") == 0)
    {
        DBG_PRINT("[REPL] LED_ON\n");
        Sys_SetLedOverride(1u);
        /* Stop blink engine first so Timer ISR won't override the pin */
        SetGreenLedMode(0, 0);
        SetGreenLed(1u);
        repl_send_ok("LED_ON", "1");
        return;
    }
    if (strcmp(args, "LED_OFF") == 0)
    {
        DBG_PRINT("[REPL] LED_OFF\n");
        Sys_SetLedOverride(1u);
        /* Stop blink engine first so Timer ISR won't override the pin */
        SetGreenLedMode(0, 0);
        SetGreenLed(0u);
        repl_send_ok("LED_OFF", "0");
        return;
    }
    if (strncmp(args, "LED_BLINK", 9) == 0)
    {
        float freq = 2.0f;
        float duty = 0.5f;
        const char *p = args + 9;
        if (*p == ',')
        {
            p++;
            freq = (float)atof(p);
            const char *comma = strchr(p, ',');
            if (comma)
            {
                duty = (float)atof(comma + 1);
            }
        }
        if (freq <= 0.0f)
        {
            repl_send_err(ERR_PARAM, "LED_BLINK_FREQ");
            return;
        }
        DBG_PRINT("[REPL] LED_BLINK -> SetGreenLedMode(freq=%.2f,duty=%.2f)\n", freq, duty);
        Sys_SetLedOverride(1u);
        SetGreenLedMode(freq, duty);
        repl_send_ok("LED_BLINK", "APPLIED");
        return;
    }

    repl_send_err(ERR_CMD, "LED");
}

static void handle_buzzer_cmd(const char *args)
{
    if (strcmp(args, "BUZZER_ON") == 0)
    {
        Buzzer_Start(1000u);
        repl_send_ok("BUZZER_ON", "1000");
        return;
    }
    if (strcmp(args, "BUZZER_OFF") == 0)
    {
        Buzzer_Stop();
        repl_send_ok("BUZZER_OFF", "0");
        return;
    }
    if (strncmp(args, "BUZZER_BEEP", 11) == 0)
    {
        uint32_t freq = 1500u;
        uint32_t dur = 150u;
        const char *p = args + 11;
        if (*p == ',')
        {
            p++;
            freq = (uint32_t)strtoul(p, NULL, 10);
            const char *comma = strchr(p, ',');
            if (comma)
            {
                dur = (uint32_t)strtoul(comma + 1, NULL, 10);
            }
        }
        if (freq == 0u || dur == 0u)
        {
            repl_send_err(ERR_PARAM, "BUZZER_BEEP");
            return;
        }
        BuzzerPlay(freq, dur);
        repl_send_ok("BUZZER_BEEP", "DONE");
        return;
    }

    repl_send_err(ERR_CMD, "BUZZER");
}

static void handle_sensor_read(void)
{
    int16_t axis[3] = {0};
    GsensorReadAxis(axis);
    repl_send("+OK,SENSOR_READ,%d,%d,%d\r\n", axis[0], axis[1], axis[2]);
}

static void handle_sensor_stream(const char *args)
{
    if (strncmp(args, "SENSOR_STREAM,QUERY", 19) == 0)
    {
        repl_send("+OK,SENSOR_STREAM,QUERY,EN=%u,SRC=%s,INT=%lu\r\n",
                  (unsigned)s_repl.stream_enabled,
                  stream_source_name(s_repl.stream_source),
                  (unsigned long)s_repl.stream_interval_ms);
        return;
    }

    if (strncmp(args, "SENSOR_STREAM,STOP", 18) == 0)
    {
        s_repl.stream_enabled = 0u;
        repl_send_ok("SENSOR_STREAM", "STOP");
        return;
    }

    if (strncmp(args, "SENSOR_STREAM,START", 19) == 0)
    {
        uint8_t source = s_repl.stream_source;
        uint32_t interval = s_repl.stream_interval_ms;
        if (args[19] == ',')
        {
            char params[64];
            char *token1;
            char *token2;
            char *comma;

            strncpy(params, args + 20, sizeof(params) - 1u);
            params[sizeof(params) - 1u] = '\0';

            comma = strchr(params, ',');
            if (comma)
            {
                *comma = '\0';
                token1 = params;
                token2 = comma + 1;
            }
            else
            {
                token1 = params;
                token2 = NULL;
            }

            if (token1 && token1[0] != '\0')
            {
                if (parse_stream_source(token1, &source))
                {
                    if (token2 && token2[0] != '\0')
                    {
                        if (!is_decimal_number(token2))
                        {
                            repl_send_err(ERR_PARAM, "SENSOR_STREAM_INTERVAL");
                            return;
                        }
                        interval = (uint32_t)strtoul(token2, NULL, 10);
                    }
                }
                else
                {
                    if (!is_decimal_number(token1))
                    {
                        repl_send_err(ERR_PARAM, "SENSOR_STREAM_SOURCE");
                        return;
                    }
                    interval = (uint32_t)strtoul(token1, NULL, 10);
                }
            }
        }
        interval = clamp_stream_interval(interval);
        s_repl.stream_source = source;
        s_repl.stream_interval_ms = interval;
        s_repl.stream_enabled = 1u;
        s_repl.last_stream_ms = get_ticks_ms();
        repl_send("+OK,SENSOR_STREAM,START,%s,%lu\r\n", stream_source_name(source), (unsigned long)interval);
        return;
    }

    repl_send_err(ERR_PARAM, "SENSOR_STREAM");
}

static void handle_adc_read(void)
{
    uint16_t raw = Adc_ReadBatteryRawAvg(4u);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    repl_send("+OK,ADC_READ,%u,%.3f\r\n", (unsigned)raw, vbat);
}

static void handle_hall_read(void)
{
    uint8_t pb7_low = ((PB->PIN & BIT7) == 0u) ? 1u : 0u;
    uint8_t pb8_low = ((PB->PIN & BIT8) == 0u) ? 1u : 0u;
    repl_send("+OK,HALL_READ,PB7=%u,PB8=%u\r\n", pb7_low, pb8_low);
}

static void handle_key_read(void)
{
    uint8_t pressed = ((PB->PIN & BIT15) == 0u) ? 1u : 0u;
    repl_send("+OK,KEY_READ,PB15=%u\r\n", pressed);
}

/* ===================== PING ===================== */
static void handle_ping(void)
{
    repl_send("+OK,PING,PONG\r\n");
}

/* ===================== VERSION ===================== */
static void handle_version(void)
{
    repl_send("+OK,VERSION,%d.%d.%d,%s,%s\r\n",
              FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH,
              __DATE__, __TIME__);
}

/* =================== Data Flash =================== */
static void handle_dflash_info(void)
{
    SYS_UnlockReg();
    FMC_Open();
    uint32_t dfba = FMC_ReadDataFlashBaseAddr();
    FMC_Close();
    SYS_LockReg();
    repl_send("+OK,DFLASH_INFO,BASE=0x%08X,SIZE=0x%X,PAGE=0x%X\r\n",
              (unsigned)dfba, (unsigned)DFLASH_SIZE, (unsigned)FMC_FLASH_PAGE_SIZE);
}

static void handle_dflash_read(const char *params)
{
    /* Format: DFLASH_READ,<hex_offset>[,<word_count>]  (count default=1, max=DFLASH_MAX_RW_LEN) */
    uint32_t offset = 0u;
    uint32_t count = 1u;
    char buf[64];
    uint32_t i;

    if (!params || params[0] == '\0')
    {
        repl_send_err(ERR_PARAM, "DFLASH_READ_OFFSET");
        return;
    }

    strncpy(buf, params, sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';

    char *comma = strchr(buf, ',');
    if (comma)
    {
        *comma = '\0';
        count = (uint32_t)strtoul(comma + 1, NULL, 0);
    }
    offset = (uint32_t)strtoul(buf, NULL, 16);

    if (offset & 0x3u)
    {
        repl_send_err(ERR_ALIGN, "DFLASH_READ_OFFSET");
        return;
    }
    if (count == 0u || count > DFLASH_MAX_RW_LEN)
    {
        repl_send_err(ERR_RANGE, "DFLASH_READ_COUNT");
        return;
    }
    if (offset + count * 4u > DFLASH_SIZE)
    {
        repl_send_err(ERR_RANGE, "DFLASH_READ_END");
        return;
    }

    SYS_UnlockReg();
    FMC_Open();

    for (i = 0u; i < count; i++)
    {
        uint32_t addr = DFLASH_BASE + offset + i * 4u;
        uint32_t val = FMC_Read(addr);
        repl_send("+OK,DFLASH_READ,0x%04X,0x%08X\r\n",
                  (unsigned)(offset + i * 4u), (unsigned)val);
    }

    FMC_Close();
    SYS_LockReg();
}

static void handle_dflash_write(const char *params)
{
    /* Format: DFLASH_WRITE,<hex_offset>,<hex_value>  (single word) */
    uint32_t offset = 0u;
    uint32_t value = 0u;
    char buf[64];

    if (!params || params[0] == '\0')
    {
        repl_send_err(ERR_PARAM, "DFLASH_WRITE_PARAMS");
        return;
    }

    strncpy(buf, params, sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';

    char *comma = strchr(buf, ',');
    if (!comma)
    {
        repl_send_err(ERR_PARAM, "DFLASH_WRITE_VALUE");
        return;
    }
    *comma = '\0';
    offset = (uint32_t)strtoul(buf, NULL, 16);
    value = (uint32_t)strtoul(comma + 1, NULL, 16);

    if (offset & 0x3u)
    {
        repl_send_err(ERR_ALIGN, "DFLASH_WRITE_OFFSET");
        return;
    }
    if (offset + 4u > DFLASH_SIZE)
    {
        repl_send_err(ERR_RANGE, "DFLASH_WRITE_OFFSET");
        return;
    }

    SYS_UnlockReg();
    FMC_Open();
    FMC_ENABLE_AP_UPDATE();

    int32_t ret = FMC_Write(DFLASH_BASE + offset, value);

    FMC_DISABLE_AP_UPDATE();
    FMC_Close();
    SYS_LockReg();

    if (ret != 0)
    {
        repl_send_err(ERR_FLASH, "DFLASH_WRITE_FAIL");
        return;
    }
    repl_send("+OK,DFLASH_WRITE,0x%04X,0x%08X\r\n",
              (unsigned)offset, (unsigned)value);
}

static void handle_dflash_erase(void)
{
    SYS_UnlockReg();
    FMC_Open();
    FMC_ENABLE_AP_UPDATE();

    int32_t ret = FMC_Erase(DFLASH_BASE);

    FMC_DISABLE_AP_UPDATE();
    FMC_Close();
    SYS_LockReg();

    if (ret != 0)
    {
        repl_send_err(ERR_FLASH, "DFLASH_ERASE_FAIL");
        return;
    }
    repl_send_ok("DFLASH_ERASE", "DONE");
}

static void handle_dflash_cmd(const char *args)
{
    if (strcmp(args, "DFLASH_INFO") == 0)
    {
        handle_dflash_info();
        return;
    }
    if (strncmp(args, "DFLASH_READ,", 12) == 0)
    {
        handle_dflash_read(args + 12);
        return;
    }
    if (strcmp(args, "DFLASH_READ") == 0)
    {
        repl_send_err(ERR_PARAM, "DFLASH_READ_OFFSET");
        return;
    }
    if (strncmp(args, "DFLASH_WRITE,", 13) == 0)
    {
        if (!Sys_GetReplMode())
        {
            repl_send_err(ERR_STATE, "NOT_IN_REPL");
            return;
        }
        handle_dflash_write(args + 13);
        return;
    }
    if (strcmp(args, "DFLASH_WRITE") == 0)
    {
        repl_send_err(ERR_PARAM, "DFLASH_WRITE_PARAMS");
        return;
    }
    if (strcmp(args, "DFLASH_ERASE") == 0)
    {
        if (!Sys_GetReplMode())
        {
            repl_send_err(ERR_STATE, "NOT_IN_REPL");
            return;
        }
        handle_dflash_erase();
        return;
    }
    repl_send_err(ERR_CMD, "DFLASH");
}

/* ===================== HELP ===================== */
static void handle_help(void)
{
    /* Use repl_send_ok to keep formatting consistent and avoid overly long
       single-format calls which may be truncated in intermediate buffers. */
    repl_send_ok("HELP",
                 "PING|VERSION|REPL_START|REPL_STOP|REPL_STATE|"
                 "STATUS|STATUS_VERBOSE|"
                 "LED_ON|LED_OFF|LED_BLINK|"
                 "BUZZER_ON|BUZZER_OFF|BUZZER_BEEP|"
                 "SENSOR_READ|SENSOR_STREAM|"
                 "ADC_READ|HALL_READ|KEY_READ|"
                 "DFLASH_INFO|DFLASH_READ|DFLASH_WRITE|DFLASH_ERASE");
}

static void handle_repl_state(void)
{
    repl_send("+OK,REPL_STATE,ACTIVE=%u,STREAM_EN=%u,SRC=%s,INT=%lu\r\n",
              (unsigned)Sys_GetReplMode(),
              (unsigned)s_repl.stream_enabled,
              stream_source_name(s_repl.stream_source),
              (unsigned long)s_repl.stream_interval_ms);
}

static void handle_status(void)
{
    repl_send("+OK,STATUS,BLE=%u,GAME=%u,REPL=%u,IDLE=%u,JUMP=%u\r\n",
              (unsigned)Sys_GetBleState(),
              (unsigned)Sys_GetGameState(),
              (unsigned)Sys_GetReplMode(),
              (unsigned)Sys_GetIdleState(),
              (unsigned)Sys_GetJumpTimes());
}

static void handle_status_verbose(void)
{
    int16_t axis[3] = {0};
    uint16_t raw = Adc_ReadBatteryRawAvg(4u);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    uint8_t pb7_low = ((PB->PIN & BIT7) == 0u) ? 1u : 0u;
    uint8_t pb8_low = ((PB->PIN & BIT8) == 0u) ? 1u : 0u;
    uint8_t key_pressed = ((PB->PIN & BIT15) == 0u) ? 1u : 0u;

    GsensorReadAxis(axis);

    repl_send("+OK,STATUS_VERBOSE,BLE=%u,GAME=%u,REPL=%u,IDLE=%u,JUMP=%u,KEY=%u,PB7=%u,PB8=%u,ADC=%u,VBAT=%.3f\r\n",
              (unsigned)Sys_GetBleState(),
              (unsigned)Sys_GetGameState(),
              (unsigned)Sys_GetReplMode(),
              (unsigned)Sys_GetIdleState(),
              (unsigned)Sys_GetJumpTimes(),
              (unsigned)key_pressed,
              (unsigned)pb7_low,
              (unsigned)pb8_low,
              (unsigned)raw,
              vbat);
    repl_send("+OK,STATUS_VERBOSE_G,AX=%d,AY=%d,AZ=%d\r\n", axis[0], axis[1], axis[2]);
}

static uint8_t handle_safe_or_common_cmd(const char *cmd)
{
    if (strncmp(cmd, "LED_", 4) == 0)
    {
        handle_led_cmd(cmd);
        return 1u;
    }

    if (strncmp(cmd, "BUZZER_", 7) == 0)
    {
        handle_buzzer_cmd(cmd);
        return 1u;
    }

    if (strcmp(cmd, "SENSOR_READ") == 0)
    {
        handle_sensor_read();
        return 1u;
    }

    if (strcmp(cmd, "ADC_READ") == 0)
    {
        handle_adc_read();
        return 1u;
    }

    if (strcmp(cmd, "HALL_READ") == 0)
    {
        handle_hall_read();
        return 1u;
    }

    if (strcmp(cmd, "KEY_READ") == 0)
    {
        handle_key_read();
        return 1u;
    }

    /* Allow DataFlash info/read queries outside REPL; write/erase require REPL */
    if (strncmp(cmd, "DFLASH_INFO", 11) == 0 || strncmp(cmd, "DFLASH_READ", 11) == 0)
    {
        handle_dflash_cmd(cmd);
        return 1u;
    }

    return 0u;
}

uint8_t BleAtRepl_HandleMessage(const char *msg)
{
    char msg_local[RXBUFSIZE];
    const char *cmd;

    if (!msg)
    {
        return 0u;
    }

    strncpy(msg_local, msg, sizeof(msg_local) - 1u);
    msg_local[sizeof(msg_local) - 1u] = '\0';
    trim_line_end(msg_local);

    cmd = skip_prefix(msg_local);

    if (!cmd)
    {
        return 0u;
    }
    DBG_PRINT("[REPL] Received cmd: %s\n", cmd);

    /* --- Commands that work regardless of REPL state --- */

    if (strcmp(cmd, "PING") == 0)
    {
        handle_ping();
        return 1u;
    }

    if (strcmp(cmd, "VERSION") == 0)
    {
        handle_version();
        return 1u;
    }

    if (strcmp(cmd, "REPL_START") == 0)
    {
        handle_repl_start();
        return 1u;
    }

    if (strcmp(cmd, "REPL_STOP") == 0)
    {
        handle_repl_stop();
        return 1u;
    }

    if (strcmp(cmd, "HELP") == 0)
    {
        handle_help();
        return 1u;
    }

    if (strcmp(cmd, "STATUS") == 0)
    {
        handle_status();
        return 1u;
    }

    if (strcmp(cmd, "REPL_STATE") == 0)
    {
        handle_repl_state();
        return 1u;
    }

    if (strcmp(cmd, "STATUS_VERBOSE") == 0)
    {
        handle_status_verbose();
        return 1u;
    }

    /* --- Commands we allow outside REPL (safe/read-only or explicitly permitted) --- */
    if (handle_safe_or_common_cmd(cmd))
    {
        return 1u;
    }

    if (!BleAtRepl_IsActive() && strncmp(cmd, "SENSOR_STREAM,START", 19) == 0)
    {
        repl_activate();
        handle_sensor_stream(cmd);
        return 1u;
    }

    if (!BleAtRepl_IsActive() && strncmp(cmd, "SENSOR_STREAM,STOP", 18) == 0)
    {
        s_repl.stream_enabled = 0u;
        repl_send_ok("SENSOR_STREAM", "STOP");
        return 1u;
    }

    if (!BleAtRepl_IsActive() && strncmp(cmd, "SENSOR_STREAM,QUERY", 19) == 0)
    {
        handle_sensor_stream(cmd);
        return 1u;
    }

    if (!BleAtRepl_IsActive())
    {
        repl_send_err(ERR_STATE, "NOT_IN_REPL");
        return 1u;
    }

    if (handle_safe_or_common_cmd(cmd))
    {
        return 1u;
    }

    if (strncmp(cmd, "SENSOR_STREAM", 13) == 0)
    {
        handle_sensor_stream(cmd);
        return 1u;
    }

    if (strncmp(cmd, "DFLASH_", 7) == 0)
    {
        handle_dflash_cmd(cmd);
        return 1u;
    }

    repl_send_err(ERR_CMD, "UNKNOWN");
    return 1u;
}

void BleAtRepl_RunIfActive(void)
{
    if (!BleAtRepl_IsActive())
    {
        return;
    }

    if (Sys_GetBleState() != BLE_CONNECTED)
    {
        s_repl.stream_enabled = 0u;
        Sys_SetReplMode(0u);
        return;
    }

    if (s_repl.stream_enabled && is_timeout(s_repl.last_stream_ms, s_repl.stream_interval_ms))
    {
        s_repl.last_stream_ms = get_ticks_ms();
        switch (s_repl.stream_source)
        {
        case REPL_STREAM_SRC_ADC:
        {
            uint16_t raw = Adc_ReadBatteryRawAvg(4u);
            float vbat = Adc_ConvertRawToBatteryV(raw);
            repl_send("+DATA,ADC,%u,%.3f\r\n", (unsigned)raw, vbat);
        }
        break;
        case REPL_STREAM_SRC_HALL:
        {
            uint8_t pb7_low = ((PB->PIN & BIT7) == 0u) ? 1u : 0u;
            uint8_t pb8_low = ((PB->PIN & BIT8) == 0u) ? 1u : 0u;
            repl_send("+DATA,HALL,PB7=%u,PB8=%u\r\n", pb7_low, pb8_low);
        }
        break;
        case REPL_STREAM_SRC_KEY:
        {
            uint8_t pressed = ((PB->PIN & BIT15) == 0u) ? 1u : 0u;
            repl_send("+DATA,KEY,PB15=%u\r\n", pressed);
        }
        break;
        case REPL_STREAM_SRC_GSENSOR:
        default:
        {
            int16_t axis[3] = {0};
            GsensorReadAxis(axis);
            repl_send("+DATA,GSENSOR,%d,%d,%d\r\n", axis[0], axis[1], axis[2]);
        }
        break;
        }
    }
}

#else

void BleAtRepl_Init(void) {}
uint8_t BleAtRepl_IsActive(void) { return 0u; }
uint8_t BleAtRepl_HandleMessage(const char *msg)
{
    (void)msg;
    return 0u;
}
void BleAtRepl_RunIfActive(void) {}

#endif
