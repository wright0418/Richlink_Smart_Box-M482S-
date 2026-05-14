/**
 * @file ble.c
 * @brief BLE UART transport, receive queue, and command processing.
 *
 * This module handles UART1 input from the BLE module, buffering complete
 * lines in an ISR-safe queue and converting them into higher-level BLE
 * command events for the main application.
 */
#include <stdarg.h>
#if defined(__has_include)
#if __has_include(<stdio.h>)
#include <stdio.h>
#else
/* Provide minimal printf/vsnprintf prototypes for host/static analysis */
int printf(const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif
#else
#include <stdio.h>
#endif

#if defined(__has_include)
#if __has_include(<string.h>)
#include <string.h>
#else
/* Minimal string functions used by this file */
char *strstr(const char *haystack, const char *needle);
size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
#endif
#else
#include <string.h>
#endif
#if defined(__has_include)
#if __has_include("NuMicro.h")
#include "NuMicro.h"
#else
#include "host_compat.h"
#endif
#else
#include "NuMicro.h"
#endif

#include "ble.h"
#include "protocol/ble_parser.h"
#include "ble_at_repl.h"
#include "system_status.h"
#include "drivers/buzzer.h"
#include "drivers/led.h"
#include "app/game_logic.h"
#include "drivers/timer.h"
#if USE_MOLE_GAME
#include "app/mole_game.h"
#endif

#if MOLE_TEST_BLE_TRACE_ENABLE
#define BLE_TRACE_PRINT(fmt, ...) printf("[MOLE_TEST] " fmt, ##__VA_ARGS__)
#else
#define BLE_TRACE_PRINT(fmt, ...)
#endif

static const char *BLE_CmdTypeToString(BleCmdType type)
{
  switch (type)
  {
  case BLE_CMD_CONNECTED:
    return "CONNECTED";
  case BLE_CMD_DISCONNECTED:
    return "DISCONNECTED";
  case BLE_CMD_CMD_MODE:
    return "CMD_MODE";
  case BLE_CMD_DATA_MODE:
    return "DATA_MODE";
  case BLE_CMD_CONN_START:
    return "CONN_START";
  case BLE_CMD_GET_CYCLE:
    return "GET_CYCLE";
  case BLE_CMD_SET_END:
    return "SET_END";
  case BLE_CMD_DISC_MSG:
    return "DISC_MSG";
  case BLE_CMD_MAC_ADDR:
    return "MAC_ADDR";
  case BLE_CMD_DEVICE_NAME:
    return "DEVICE_NAME";
  case BLE_CMD_NONE:
  default:
    return "NONE";
  }
}

/* Reuse buffer sizes from header */
volatile uint8_t g_u8RecData[RXBUFSIZE] = {0};
volatile uint32_t g_u32RecLen = 0;
volatile uint8_t g_u8DataReady = 0;
static volatile uint8_t s_uart_line_buf[RXBUFSIZE] = {0};
static volatile uint32_t s_uart_line_len = 0;
static volatile uint8_t s_uart_line_overflow = 0;

#define BLE_LINE_QUEUE_DEPTH 4u
static volatile uint8_t s_line_queue[BLE_LINE_QUEUE_DEPTH][RXBUFSIZE] = {0};
static volatile uint16_t s_line_queue_len[BLE_LINE_QUEUE_DEPTH] = {0};
static volatile uint8_t s_line_q_head = 0u;
static volatile uint8_t s_line_q_tail = 0u;
static volatile uint8_t s_line_q_count = 0u;

#define BLE_RAW_RX_BUF_SIZE RXBUFSIZE
static volatile uint8_t s_raw_rx_buf[BLE_RAW_RX_BUF_SIZE] = {0};
static volatile uint16_t s_raw_rx_head = 0u;
static volatile uint16_t s_raw_rx_tail = 0u;

static void BLE_DebugDumpBytes(const char *tag, const uint8_t *data, uint32_t len, uint32_t max_show)
{
#if MOLE_TEST_BLE_TRACE_ENABLE
  if ((tag == NULL) || (data == NULL) || (len == 0u))
  {
    return;
  }

  uint32_t n = (len > max_show) ? max_show : len;
  BLE_TRACE_PRINT("%s len=%lu bytes=%02X", tag, (unsigned long)len, (unsigned)data[0]);
  for (uint32_t i = 1u; i < n; i++)
  {
    printf(" %02X", (unsigned)data[i]);
  }
  if (len > n)
  {
    printf(" ...");
  }
  printf("\r\n");
#else
  (void)tag;
  (void)data;
  (void)len;
  (void)max_show;
#endif
}

/**
 * @brief Push a received byte into the raw BLE UART ring buffer.
 *
 * Called from UART1 IRQ context, so it must not block and must keep the
 * buffer in a consistent state even when overflow occurs.
 */
static void BLE_RawRxPush(uint8_t byte)
{
  uint16_t next = (uint16_t)((s_raw_rx_head + 1u) % BLE_RAW_RX_BUF_SIZE);

  if (next == s_raw_rx_tail)
  {
    /* Drop oldest byte to keep the ISR non-blocking and preserve latest stream. */
    s_raw_rx_tail = (uint16_t)((s_raw_rx_tail + 1u) % BLE_RAW_RX_BUF_SIZE);
  }

  s_raw_rx_buf[s_raw_rx_head] = byte;
  s_raw_rx_head = next;
}

/**
 * @brief Enqueue a complete received line from ISR context.
 *
 * Drops the oldest queued line when the queue is full in order to keep the
 * ISR fast and preserve the most recent input.
 */
static void BLE_LineQueuePushFromIsr(const uint8_t *line, uint32_t len)
{
  if ((line == NULL) || (len == 0u))
  {
    return;
  }

  uint32_t copy_len = len;
  if (copy_len >= RXBUFSIZE)
  {
    copy_len = RXBUFSIZE - 1u;
  }

  if (s_line_q_count >= BLE_LINE_QUEUE_DEPTH)
  {
    /* Queue full: drop oldest line and keep latest, so critical state
       messages (e.g. CONNECTED/DISCONNECTED) are less likely to be lost. */
    s_line_q_tail = (uint8_t)((s_line_q_tail + 1u) % BLE_LINE_QUEUE_DEPTH);
    s_line_q_count--;
  }

  uint8_t idx = s_line_q_head;
  memcpy((void *)s_line_queue[idx], (const void *)line, copy_len);
  s_line_queue[idx][copy_len] = '\0';
  s_line_queue_len[idx] = (uint16_t)copy_len;
  s_line_q_head = (uint8_t)((s_line_q_head + 1u) % BLE_LINE_QUEUE_DEPTH);
  s_line_q_count++;

  /* Keep legacy exported snapshot updated for observers. */
  memcpy((void *)g_u8RecData, (const void *)s_line_queue[idx], copy_len + 1u);
  g_u32RecLen = copy_len;
  g_u8DataReady = 1u;
}

void Ble_RawRxReset(void)
{
  __disable_irq();
  s_raw_rx_head = 0u;
  s_raw_rx_tail = 0u;
  __enable_irq();
}

uint32_t Ble_TakeRawBytes(uint8_t *dst, uint32_t max_len)
{
  uint32_t copied = 0u;

  if ((dst == NULL) || (max_len == 0u))
  {
    return 0u;
  }

  __disable_irq();
  while ((s_raw_rx_tail != s_raw_rx_head) && (copied < max_len))
  {
    dst[copied++] = s_raw_rx_buf[s_raw_rx_tail];
    s_raw_rx_tail = (uint16_t)((s_raw_rx_tail + 1u) % BLE_RAW_RX_BUF_SIZE);
  }
  __enable_irq();

  if (copied > 0u)
  {
    /* Receive path debug: BLE module/UART1 -> MCU raw bytes */
    BLE_DebugDumpBytes("UART1 RX RAW", dst, copied, 12u);
  }

  return copied;
}

static size_t BLE_TrimLineLen(const char *src, size_t len)
{
  while (len > 0u && (src[len - 1u] == '\r' || src[len - 1u] == '\n'))
  {
    len--;
  }
  return len;
}

static uint8_t BLE_NormalizeAndHandleRepl(char *msg)
{
  if (!msg)
  {
    return 0u;
  }

  BleParser_StripCmdModeMarker(msg, BLE_CMD_CCMD);

  /* BLE module/system replies (CMD mode ACK, name/MAC query results, etc.)
     must continue through the normal BLE parser / rename flow. If we send
     them into the REPL handler first, they can be consumed as unknown REPL
     commands before Sys_SetMacAddr()/Sys_SetDeviceName() ever sees them. */
  if (BleParser_ParseCommand((const char *)msg) != BLE_CMD_NONE)
  {
    return 0u;
  }

  return BleAtRepl_HandleMessage((const char *)msg);
}

void UART1_IRQHandler(void)
{
  uint8_t u8InChar = 0xFF;
  uint32_t u32IntSts = UART1->INTSTS;

  if (u32IntSts & UART_INTSTS_RDAIF_Msk)
  {
    while (UART_IS_RX_READY(UART1))
    {
      u8InChar = UART_READ(UART1);
      BLE_RawRxPush(u8InChar);

      if ((u8InChar == '\n') || (u8InChar == '\r'))
      {
        if (s_uart_line_len > 0u)
        {
          BLE_LineQueuePushFromIsr((const uint8_t *)s_uart_line_buf, s_uart_line_len);
        }

        s_uart_line_len = 0;
        s_uart_line_overflow = 0;
        continue;
      }

      if (!s_uart_line_overflow)
      {
        if (s_uart_line_len < (RXBUFSIZE - 1u))
        {
          s_uart_line_buf[s_uart_line_len++] = u8InChar;
        }
        else
        {
          s_uart_line_overflow = 1u;
        }
      }
    }
  }

  if (u32IntSts & UART_INTSTS_THREIF_Msk)
  {
    UART_DisableInt(UART1, UART_INTEN_THREIEN_Msk);
  }
}

/**
 * @brief Dequeue the next complete BLE message line for main-loop processing.
 * @param dst Destination buffer for the line.
 * @param dst_size Size of destination buffer.
 * @return 1 if a line was available, 0 otherwise.
 */
static uint8_t BLE_TakeMessageSnapshot(char *dst, size_t dst_size)
{
  if (!dst || dst_size == 0u)
  {
    return 0u;
  }

  uint8_t has_data = 0u;

  __disable_irq();
  if (s_line_q_count > 0u)
  {
    uint8_t idx = s_line_q_tail;
    size_t copy_len = (size_t)s_line_queue_len[idx];
    if (copy_len >= dst_size)
    {
      copy_len = dst_size - 1u;
    }

    for (size_t i = 0u; i < copy_len; i++)
    {
      dst[i] = (char)s_line_queue[idx][i];
    }
    dst[copy_len] = '\0';

    s_line_q_tail = (uint8_t)((s_line_q_tail + 1u) % BLE_LINE_QUEUE_DEPTH);
    s_line_q_count--;

    if (s_line_q_count > 0u)
    {
      uint8_t next_idx = s_line_q_tail;
      uint32_t next_len = (uint32_t)s_line_queue_len[next_idx];
      memcpy((void *)g_u8RecData, (const void *)s_line_queue[next_idx], next_len + 1u);
      g_u32RecLen = next_len;
      g_u8DataReady = 1u;
    }
    else
    {
      g_u8DataReady = 0u;
      g_u32RecLen = 0u;
    }

    has_data = 1u;
  }
  __enable_irq();

  return has_data;
}

int BLE_UART_SEND(void *uart, const char *format, ...)
{
  uint8_t buf[BUF_SIZE];
  va_list arg;
  int done;
  int write_len;

  if (uart == NULL || format == NULL)
  {
    return -1;
  }

  va_start(arg, format);
  done = vsnprintf((char *)buf, BUF_SIZE, format, arg);
  va_end(arg);

  if (done < 0)
  {
    return done;
  }

  /* vsnprintf returns the length that would have been written (excluding '\0').
     Clamp to local buffer capacity to avoid reading past buf on UART_Write(). */
  write_len = done;
  if (write_len >= (int)BUF_SIZE)
  {
    write_len = (int)BUF_SIZE - 1;
  }

  /* uart is an opaque pointer in the public API; cast back to UART_T* for SDK calls */
  UART_T *u = (UART_T *)uart;
  UART_Write(u, buf, write_len);

  /* Transmit path debug: MCU -> BLE module/UART1 (AT/text command path) */
  BLE_DebugDumpBytes("UART1 TX TXT", buf, (uint32_t)write_len, 32u);
  return write_len;
}

/**
 * @brief Process one pending BLE line from the receive queue.
 *
 * If a complete line is available, it is parsed and converted into the
 * appropriate system state update or application event.
 */
void CheckBleRecvMsg(void)
{
  char msg[RXBUFSIZE];
  uint8_t i;

  if (BLE_TakeMessageSnapshot(msg, sizeof(msg)))
  {
    BLE_TRACE_PRINT("UART1 RX LINE: %s\r\n", msg);

    if (BLE_NormalizeAndHandleRepl(msg))
    {
      return;
    }

    BleCmdType cmdType = BleParser_ParseCommand((const char *)msg);
    BLE_TRACE_PRINT("BLE TEXT PARSER result=%s(%u) line=%s\r\n",
                    BLE_CmdTypeToString(cmdType),
                    (unsigned)cmdType,
                    msg);

    switch (cmdType)
    {
    case BLE_CMD_CONNECTED:
#if USE_MOLE_GAME
      MoleGame_ResetFrameState();
#endif
      Sys_SetBleState(BLE_CONNECTED);
      /* Reset movement inactivity timer on BLE connect so user hold doesn't
        immediately trigger idle state. */
      Game_ResetMovementTimer();
      Game_ResetBleTimer(); /* Reset BLE send timer on reconnect */
      break;
    case BLE_CMD_DISCONNECTED:
#if USE_MOLE_GAME
      MoleGame_ResetFrameState();
#endif
      Sys_SetBleState(BLE_DISCONNECTED);
      Sys_SetGameState(GAME_STOP);
      /* BLE disconnected: restart idle countdown (rule 4 - 30s) */
      Game_ResetMovementTimer();
      break;
    case BLE_CMD_CMD_MODE:
      Sys_SetBleMode(0u);
      break;
    case BLE_CMD_DATA_MODE:
      Sys_SetBleMode(1u);
      break;
    case BLE_CMD_CONN_START:
      BLE_UART_SEND((void *)UART1, "Connecting\n");
      Sys_ResetJumpTimes();
      break;
    case BLE_CMD_GET_CYCLE:
      Sys_ResetJumpTimes();
      BLE_UART_SEND((void *)UART1, "Connecting\n");
      BuzzerPlay(2000, 800);
      Sys_SetGameState(GAME_START);
      break;
    case BLE_CMD_SET_END:
      Sys_SetGameState(GAME_STOP);
      Sys_ResetJumpTimes();
      /* Game ended: restart idle countdown (rule 2) */
      Game_ResetMovementTimer();
      for (i = 0; i < 5; i++)
      {
        BuzzerPlay(1000, 200);
        delay_ms(200);
      }
      break;
    case BLE_CMD_DISC_MSG:
      Sys_SetGameState(GAME_STOP);
      Sys_ResetJumpTimes();
      Game_ResetMovementTimer();
      break;
    case BLE_CMD_MAC_ADDR:
    {
      size_t len = strlen((const char *)msg);
      len = BLE_TrimLineLen((const char *)msg, len);
      if (len > 0u)
      {
        Sys_SetMacAddr((const char *)msg, (uint32_t)len);
      }
    }
    break;
    case BLE_CMD_DEVICE_NAME:
    {
      size_t len = strlen((const char *)msg);
      len = BLE_TrimLineLen((const char *)msg, len);
      if (len > 0u)
      {
        Sys_SetDeviceName((const char *)msg, (uint32_t)len);
      }
    }
    break;
    case BLE_CMD_NONE:
    default:
      break;
    }
  }
}

void ble_send_cmd(const char *cmd, uint32_t delay)
{
  BLE_UART_SEND((void *)UART1, "%s", cmd);
  delay_ms(delay);
}

void BLE_DISCONNECT()
{
  ble_send_cmd(BLE_CMD_CCMD, 500);
  ble_send_cmd("AT+DISC\r\n", 100);
  ble_send_cmd("AT+MODE_DATA\r\n", 0);
}

void BLE_to_DLPS()
{
  ble_send_cmd(BLE_CMD_CCMD, 200);
  ble_send_cmd(BLE_CMD_ADVERT_OFF, 20);
  ble_send_cmd(BLE_CMD_DLPS_ON, 20);
}

void BLEToRunMode()
{
  /* Enter CMD mode first so following AT commands are guaranteed to apply.
     Without this, EN_SYSMSG can be silently ignored in DATA mode and
     CONNECTED/DISCONNECTED notifications will never be emitted. */
  ble_send_cmd(BLE_CMD_CCMD, 120);
  ble_send_cmd(BLE_CMD_DLPS_OFF, 100);
  ble_send_cmd(BLE_CMD_EN_SYSMSG_ON, 50);
  ble_send_cmd(BLE_CMD_ADVERT_ON, 20);
  ble_send_cmd(BLE_CMD_MODE_DATA, 200);
}

void BLEDisconnect()
{
  ble_send_cmd(BLE_CMD_CCMD, 500);
  ble_send_cmd(BLE_CMD_DISC, 100);
  ble_send_cmd(BLE_CMD_MODE_DATA, 100);
}

void BLESetName(const char *name)
{
  BLE_UART_SEND((void *)UART1, "AT+NAME=%s\r\n", name);
  delay_ms(100);
}

void BLESendData(const char *data)
{
  BLE_UART_SEND((void *)UART1, data);
  delay_ms(2);
}

void BLESendBytes(const uint8_t *data, uint32_t len)
{
  if ((data == NULL) || (len == 0u))
  {
    return;
  }

  UART_Write(UART1, (uint8_t *)data, len);

  /* Transmit path debug: MCU -> BLE module/UART1 (binary payload path) */
  BLE_DebugDumpBytes("UART1 TX BIN", data, len, 32u);
  delay_ms(2);
}

void BleSetup(void)
{
  BLEToRunMode();
}

/* ---- BLE rename flow helpers (moved from main.c) ---- */

typedef enum
{
  BLE_RENAME_IDLE = 0,
  BLE_RENAME_SEND_CCMD,
  BLE_RENAME_WAIT_CCMD,
  BLE_RENAME_SEND_NAME_QUERY,
  BLE_RENAME_WAIT_NAME,
  BLE_RENAME_SEND_ADDR_QUERY,
  BLE_RENAME_WAIT_ADDR,
  BLE_RENAME_SEND_SET_NAME,
  BLE_RENAME_WAIT_SET_NAME,
  BLE_RENAME_SEND_REBOOT,
  BLE_RENAME_WAIT_REBOOT,
  BLE_RENAME_SEND_MODE_DATA,
  BLE_RENAME_DONE
} BleRenameState;

typedef struct
{
  uint8_t active;
  uint8_t done;
  uint8_t need_rename;
  uint8_t ccmd_retry;
  uint32_t state_enter_ms;
  BleRenameState state;
  char mac_suffix[5];
  char name_suffix[5];
  uint8_t has_name_suffix;
} BleRenameFlowCtx;

static BleRenameFlowCtx s_rename = {0};

void Ble_RenameFlow(uint8_t *device_name, uint8_t *mac)
{
  /* Backward-compatible wrapper: run non-blocking flow with bounded wait. */
  uint32_t start = get_ticks_ms();
  Ble_RenameFlowStart();
  while (!Ble_RenameFlowIsDone() && !is_timeout(start, 2000u))
  {
    CheckBleRecvMsg();
    Ble_RenameFlowProcess();
    delay_ms(5u);
  }

  if (device_name)
  {
    memcpy(device_name, (const void *)s_rename.mac_suffix, 4u);
    device_name[4] = '\0';
  }
  if (mac)
  {
    memcpy(mac, (const void *)s_rename.mac_suffix, 4u);
    mac[4] = '\0';
  }
}

void Ble_RenameFlowStart(void)
{
  memset((void *)&s_rename, 0, sizeof(s_rename));
  s_rename.active = 1u;
  s_rename.done = 0u;
  s_rename.need_rename = 0u;
  s_rename.ccmd_retry = 0u;
  s_rename.state = BLE_RENAME_SEND_CCMD;
  s_rename.state_enter_ms = get_ticks_ms();
  s_rename.mac_suffix[0] = '\0';

  /* Force WAIT_CCMD to rely on fresh BLE reply instead of stale mode value. */
  Sys_SetBleMode(1u);
  Sys_ClearMacAddr();
  Sys_ClearDeviceName();
}

uint8_t Ble_RenameFlowIsDone(void)
{
  return s_rename.done;
}

void Ble_RenameFlowProcess(void)
{
  char device_name[SYS_DEVICE_NAME_BUF_SIZE];
  char mac_addr[SYS_MAC_ADDR_BUF_SIZE];

  if (!s_rename.active || s_rename.done)
  {
    return;
  }

  uint32_t now = get_ticks_ms();

  switch (s_rename.state)
  {
  case BLE_RENAME_SEND_CCMD:
    BLE_UART_SEND(UART1, BLE_CMD_CCMD);
    s_rename.state = BLE_RENAME_WAIT_CCMD;
    s_rename.state_enter_ms = now;
    break;

  case BLE_RENAME_WAIT_CCMD:
    if (Sys_GetBleMode() == 0)
    {
      s_rename.state = BLE_RENAME_SEND_NAME_QUERY;
      s_rename.state_enter_ms = now;
    }
    else if (is_timeout(s_rename.state_enter_ms, 400u))
    {
      if (s_rename.ccmd_retry == 0u)
      {
        s_rename.ccmd_retry = 1u;
        s_rename.state = BLE_RENAME_SEND_CCMD;
        s_rename.state_enter_ms = now;
      }
      else
      {
        /* Continue to query flow even without explicit mode ACK. */
        s_rename.state = BLE_RENAME_SEND_NAME_QUERY;
        s_rename.state_enter_ms = now;
      }
    }
    break;

  case BLE_RENAME_SEND_NAME_QUERY:
    Sys_ClearDeviceName();
    BLE_UART_SEND(UART1, BLE_CMD_NAME_QUERY);
    s_rename.state = BLE_RENAME_WAIT_NAME;
    s_rename.state_enter_ms = now;
    break;

  case BLE_RENAME_WAIT_NAME:
    if (Sys_CopyDeviceName(device_name, (uint32_t)sizeof(device_name)) > 0u)
    {
      if (BleParser_IsNameQueryEcho(device_name))
      {
        Sys_ClearDeviceName();
        break;
      }

      /* Always compare current name suffix with MAC suffix so we can skip rename
         when the BLE module already matches the expected profile-specific name.
         This also allows repair of old/legacy names from prior firmware releases. */
#if USE_MOLE_GAME
      s_rename.has_name_suffix = BleParser_ExtractNameSuffix4(device_name, MOLE_BLE_NAME_PREFIX, s_rename.name_suffix);
#elif USE_SQUAT_MODE
      s_rename.has_name_suffix = BleParser_ExtractNameSuffix4(device_name, SPORT_BLE_NAME_PREFIX, s_rename.name_suffix);
#else
      s_rename.has_name_suffix = BleParser_ExtractRopeSuffix4(device_name, s_rename.name_suffix);
#endif
      s_rename.state = BLE_RENAME_SEND_ADDR_QUERY;
      s_rename.state_enter_ms = now;
    }
    else if (is_timeout(s_rename.state_enter_ms, 300u))
    {
      /* No valid name response in time -> still query MAC and decide. */
      s_rename.has_name_suffix = 0u;
      s_rename.name_suffix[0] = '\0';
      s_rename.state = BLE_RENAME_SEND_ADDR_QUERY;
      s_rename.state_enter_ms = now;
    }
    break;

  case BLE_RENAME_SEND_ADDR_QUERY:
    Sys_ClearMacAddr();
    BLE_UART_SEND(UART1, BLE_CMD_ADDR_QUERY);
    s_rename.state = BLE_RENAME_WAIT_ADDR;
    s_rename.state_enter_ms = now;
    break;

  case BLE_RENAME_WAIT_ADDR:
    if (Sys_CopyMacAddr(mac_addr, (uint32_t)sizeof(mac_addr)) > 0u)
    {
      if (BleParser_IsAddrQueryEcho(mac_addr))
      {
        Sys_ClearMacAddr();
        break;
      }

      uint8_t has_suffix = BleParser_ExtractMacSuffix4(mac_addr, s_rename.mac_suffix);
      if (has_suffix)
      {
        if (s_rename.has_name_suffix &&
            strncmp((const char *)s_rename.name_suffix,
                    (const char *)s_rename.mac_suffix, 4u) == 0)
        {
          /* Name already matches MAC suffix. */
          s_rename.need_rename = 0u;
          s_rename.state = BLE_RENAME_SEND_MODE_DATA;
        }
        else
        {
          /* Name missing/invalid/mismatched: repair name. */
          s_rename.need_rename = 1u;
          s_rename.state = BLE_RENAME_SEND_SET_NAME;
        }
        DBG_PRINT("[BLE] rename check name_suffix=%s mac_suffix=%s need_rename=%u\n",
                  s_rename.has_name_suffix ? s_rename.name_suffix : "NONE",
                  s_rename.mac_suffix,
                  (unsigned)s_rename.need_rename);
        s_rename.state_enter_ms = now;
      }
      else
      {
        /* Response arrived but not parseable yet, keep waiting until timeout. */
        Sys_ClearMacAddr();
      }
    }
    else if (is_timeout(s_rename.state_enter_ms, 400u))
    {
      DBG_PRINT("[BLE] WARN: MAC suffix not found, skip rename\n");
      s_rename.state = BLE_RENAME_SEND_MODE_DATA;
      s_rename.state_enter_ms = now;
    }
    break;

  case BLE_RENAME_SEND_SET_NAME:
    /* Send the profile-specific BLE device name for rename.
       Squat mode uses SPORT_XXXX, Mole mode uses MOLE_XXXX, and legacy rope
       firmware still uses ROPE_XXXX for backward compatibility. */
#if USE_MOLE_GAME
    BLE_UART_SEND(UART1, "AT+NAME=%s%s\r\n", MOLE_BLE_NAME_PREFIX, s_rename.mac_suffix);
#elif USE_SQUAT_MODE
    BLE_UART_SEND(UART1, "AT+NAME=%s%s\r\n", SPORT_BLE_NAME_PREFIX, s_rename.mac_suffix);
#else
    BLE_UART_SEND(UART1, "AT+NAME=ROPE_%s\r\n", s_rename.mac_suffix);
#endif
    s_rename.state = BLE_RENAME_WAIT_SET_NAME;
    s_rename.state_enter_ms = now;
    break;

  case BLE_RENAME_WAIT_SET_NAME:
    if (is_timeout(s_rename.state_enter_ms, 200u))
    {
      s_rename.state = BLE_RENAME_SEND_REBOOT;
      s_rename.state_enter_ms = now;
    }
    break;

  case BLE_RENAME_SEND_REBOOT:
    BLE_UART_SEND(UART1, BLE_CMD_REBOOT);
    s_rename.state = BLE_RENAME_WAIT_REBOOT;
    s_rename.state_enter_ms = now;
    break;

  case BLE_RENAME_WAIT_REBOOT:
    if (is_timeout(s_rename.state_enter_ms, 500u))
    {
      s_rename.state = BLE_RENAME_SEND_MODE_DATA;
      s_rename.state_enter_ms = now;
    }
    break;

  case BLE_RENAME_SEND_MODE_DATA:
    BLE_UART_SEND(UART1, BLE_CMD_MODE_DATA);
    s_rename.state = BLE_RENAME_DONE;
    s_rename.state_enter_ms = now;
    DBG_PRINT("rename flow done\n");
    break;

  case BLE_RENAME_DONE:
  default:
    s_rename.done = 1u;
    s_rename.active = 0u;
    break;
  }
}

void Ble_Init(uint32_t baud)
{
  /* Open UART1 for BLE transport and enable RX interrupt */
  UART_Open(UART1, baud);
  NVIC_EnableIRQ(UART1_IRQn);
  UART_EnableInt(UART1, UART_INTEN_RDAIEN_Msk | UART_INTEN_THREIEN_Msk);
}
