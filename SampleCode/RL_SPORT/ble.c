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

/* Reuse buffer sizes from header */
volatile uint8_t g_u8RecData[RXBUFSIZE] = {0};
volatile uint32_t g_u32RecLen = 0;
volatile uint8_t g_u8DataReady = 0;
static volatile uint8_t s_uart_line_buf[RXBUFSIZE] = {0};
static volatile uint32_t s_uart_line_len = 0;
static volatile uint8_t s_uart_line_overflow = 0;

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

      if (u8InChar == '\n')
      {
        if (!g_u8DataReady)
        {
          uint32_t copy_len = s_uart_line_len;
          if (copy_len >= RXBUFSIZE)
          {
            copy_len = RXBUFSIZE - 1u;
          }

          memcpy((void *)g_u8RecData, (const void *)s_uart_line_buf, copy_len);
          g_u8RecData[copy_len] = '\0';
          g_u32RecLen = copy_len;
          g_u8DataReady = 1;
        }

        s_uart_line_len = 0;
        s_uart_line_overflow = 0;
        break;
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

static uint8_t BLE_TakeMessageSnapshot(char *dst, size_t dst_size)
{
  if (!dst || dst_size == 0u)
  {
    return 0u;
  }

  uint8_t has_data = 0u;

  __disable_irq();
  if (g_u8DataReady)
  {
    size_t copy_len = (size_t)g_u32RecLen;
    if (copy_len >= dst_size)
    {
      copy_len = dst_size - 1u;
    }

    for (size_t i = 0u; i < copy_len; i++)
    {
      dst[i] = (char)g_u8RecData[i];
    }
    dst[copy_len] = '\0';

    g_u8DataReady = 0;
    g_u32RecLen = 0;
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
  return write_len;
}

void CheckBleRecvMsg(void)
{
  char msg[RXBUFSIZE];
  uint8_t i;

  if (BLE_TakeMessageSnapshot(msg, sizeof(msg)))
  {
    if (BLE_NormalizeAndHandleRepl(msg))
    {
      return;
    }

    BleCmdType cmdType = BleParser_ParseCommand((const char *)msg);
    switch (cmdType)
    {
    case BLE_CMD_CONNECTED:
      Sys_SetBleState(BLE_CONNECTED);
      /* Reset movement inactivity timer on BLE connect so user hold doesn't
        immediately trigger idle state. */
      Game_ResetMovementTimer();
      Game_ResetBleTimer(); /* Reset BLE send timer on reconnect */
      break;
    case BLE_CMD_DISCONNECTED:
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
  ble_send_cmd(BLE_CMD_DLPS_OFF, 100);
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

      /* Always compare current name suffix with MAC suffix to remediate
         old FW wrong rename cases. */
      s_rename.has_name_suffix = BleParser_ExtractRopeSuffix4(device_name, s_rename.name_suffix);
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
    BLE_UART_SEND(UART1, "AT+NAME=ROPE_%s\r\n", s_rename.mac_suffix);
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
