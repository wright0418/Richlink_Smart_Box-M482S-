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
#include "system_status.h"
#include "buzzer.h"
#include "led.h"
#include "game_logic.h"

/* Reuse buffer sizes from header */
volatile uint8_t g_u8RecData[RXBUFSIZE] = {0};
volatile uint32_t g_u32RecLen = 0;
volatile uint8_t g_u8DataReady = 0;

/* BLE command enum and table (moved from main.c) */
typedef enum
{
  BLE_CMD_NONE = 0,
  BLE_CMD_CONNECTED,
  BLE_CMD_DISCONNECTED,
  BLE_CMD_CMD_MODE,
  BLE_CMD_DATA_MODE,
  BLE_CMD_CONN_START,
  BLE_CMD_GET_CYCLE,
  BLE_CMD_SET_END,
  BLE_CMD_DISC_MSG,
  BLE_CMD_MAC_ADDR,
  BLE_CMD_DEVICE_NAME
} BleCmdType;

static const struct
{
  BleCmdType type;
  const char *keyword;
} BleCmdTable[] = {
    {BLE_CMD_CONNECTED, ": CONNECTED OK"},
    {BLE_CMD_DISCONNECTED, ": DISCONNECTED OK"},
    {BLE_CMD_CMD_MODE, ": CMD_MODE OK"},
    {BLE_CMD_DATA_MODE, ": DATA_MODE OK"},
    {BLE_CMD_CONN_START, "conn st"},
    {BLE_CMD_GET_CYCLE, "get cycle"},
    {BLE_CMD_SET_END, "set end"},
    {BLE_CMD_DISC_MSG, "disc"},
    {BLE_CMD_MAC_ADDR, "MAC_ADDR"},
    {BLE_CMD_DEVICE_NAME, "DEVICE_NAME"}};

BleCmdType BLEParseCommand(const char *msg)
{
  uint8_t i;
  for (i = 0; i < sizeof(BleCmdTable) / sizeof(BleCmdTable[0]); ++i)
  {
    if (strstr(msg, BleCmdTable[i].keyword))
    {
      return BleCmdTable[i].type;
    }
  }
  return BLE_CMD_NONE;
}

extern SystemStatus g_sys; /* defined in main.c via system_status.h */

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
        g_u8RecData[g_u32RecLen] = '\0';
        g_u8DataReady = 1;
        break;
      }

      g_u8RecData[g_u32RecLen] = u8InChar;
      g_u32RecLen++;
      if (g_u32RecLen >= RXBUFSIZE)
      {
        g_u32RecLen = 0;
      }
    }
  }

  if (u32IntSts & UART_INTSTS_THREIF_Msk)
  {
    UART_DisableInt(UART1, UART_INTEN_THREIEN_Msk);
  }
}

void receiveData(volatile uint8_t **recData)
{
  if (g_u8DataReady)
  {
    *recData = g_u8RecData;
    g_u8DataReady = 0;
    g_u32RecLen = 0;
  }
}

int BLE_UART_SEND(void *uart, const char *format, ...)
{
  uint8_t buf[BUF_SIZE];
  va_list arg;
  int done;

  va_start(arg, format);
  done = vsnprintf((char *)buf, BUF_SIZE, format, arg);
  va_end(arg);

  /* uart is an opaque pointer in the public API; cast back to UART_T* for SDK calls */
  UART_T *u = (UART_T *)uart;
  UART_Write(u, buf, done);
  return done;
}

void CheckBleRecvMsg(void)
{
  volatile uint8_t *pRecData, i;
  volatile uint8_t **ppRecData = &pRecData;
  if (g_u8DataReady)
  {
    receiveData(ppRecData);

#if DEBUG
    printf("[DEBUG] Received: %s\n", pRecData);
#endif

    BleCmdType cmdType = BLEParseCommand((const char *)pRecData);
    switch (cmdType)
    {
    case BLE_CMD_CONNECTED:
      g_sys.ble_state = BLE_CONNECTED;
      /* Reset movement inactivity timer on BLE connect so user hold doesn't
         immediately trigger power-down. */
      Game_ResetMovementTimer();
      break;
    case BLE_CMD_DISCONNECTED:
      g_sys.ble_state = BLE_DISCONNECTED;
      g_sys.game_state = GAME_STOP;
      break;
    case BLE_CMD_CMD_MODE:
      g_sys.ble_mode = 0;
      break;
    case BLE_CMD_DATA_MODE:
      g_sys.ble_mode = 1;
      break;
    case BLE_CMD_CONN_START:
      BLE_UART_SEND((void *)UART1, "Connecting\n");
      g_sys.jump_times = 0;
      break;
    case BLE_CMD_GET_CYCLE:
      g_sys.jump_times = 0;
      BLE_UART_SEND((void *)UART1, "Connecting\n");
      BuzzerPlay(2000, 800);
      g_sys.game_state = GAME_START;
      break;
    case BLE_CMD_SET_END:
      g_sys.game_state = GAME_STOP;
      g_sys.jump_times = 0;
      for (i = 0; i < 5; i++)
      {
        BuzzerPlay(1000, 200);
        delay_ms(200);
      }
      break;
    case BLE_CMD_DISC_MSG:
      g_sys.game_state = GAME_STOP;
      g_sys.jump_times = 0;
      break;
    case BLE_CMD_MAC_ADDR:
    {
      size_t len = strlen((const char *)pRecData);
      if (len > 0 && len <= sizeof(g_sys.mac_addr))
      {
        memcpy((void *)g_sys.mac_addr, (const char *)pRecData, len);
        g_sys.mac_addr[len < sizeof(g_sys.mac_addr) ? len : sizeof(g_sys.mac_addr) - 1] = '\0';
      }
    }
    break;
    case BLE_CMD_DEVICE_NAME:
    {
      size_t len = strlen((const char *)pRecData);
      if (len > 0 && len <= sizeof(g_sys.device_name))
      {
        memcpy((void *)g_sys.device_name, (const char *)pRecData, len);
        g_sys.device_name[len < sizeof(g_sys.device_name) ? len : sizeof(g_sys.device_name) - 1] = '\0';
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

void Ble_Init(uint32_t baud)
{
  /* Open UART1 for BLE transport and enable RX interrupt */
  UART_Open(UART1, baud);
  NVIC_EnableIRQ(UART1_IRQn);
  UART_EnableInt(UART1, UART_INTEN_RDAIEN_Msk | UART_INTEN_THREIEN_Msk);
}
