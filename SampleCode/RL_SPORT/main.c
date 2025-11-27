/*************************************************************************/
/*
 * @file     main.c
 * @version  V2.00
 * @brief    A project SPORT for M480 MCU.
 *
 * @copyright (C) 2023 Richlink  Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "NuMicro.h"

/* Project configuration and common definitions */
#include "project_config.h"
#include "system_status.h"
#include "timer.h"
#include "power_mgmt.h"
#include "game_logic.h"
#include "ble.h"
#include "led.h"
#include "buzzer.h"
#include "gpio.h"
#include "gsensor.h"
#include "i2c.h"

/* Application-specific configuration */
#define STANDBY_TIME (60 * 1000) // 60Sec = 60000ms
#define Gsensor_Addr 0x15
#define RXBUFSIZE 512
#define BUF_SIZE 512

// GPIO Pins (待移到gpio.h)
#define PIN_KEYA PB15
#define PIN_JUMP1 PB7
#define PIN_JUMP2 PB8
#define PIN_GSENSOR_INT PC5

// BLE Command Strings (已在ble.h定義)
#define BLE_CMD_NAME_QUERY "AT+NAME=?\r\n"
#define BLE_CMD_ADDR_QUERY "AT+ADDR=?\r\n"
#define BLE_CMD_MODE_DATA "AT+MODE_DATA\r\n"
#define BLE_CMD_REBOOT "AT+REBOOT\r\n"
#define BLE_CMD_DISC "AT+DISC\r\n"
#define BLE_CMD_ADVERT_ON "AT+ADVERT=1\r\n"
#define BLE_CMD_ADVERT_OFF "AT+ADVERT=0\r\n"
#define BLE_CMD_DLPS_ON "AT+DLPS_EN=1\r\n"
#define BLE_CMD_DLPS_OFF "AT+DLPS_EN=0\r\n"
#define BLE_CMD_CCMD "!CCMD@"

// BLE command enum (已在ble.c實作)
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

// BLE command string table
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

/* BLEParseCommand moved to ble.c; use the implementation from there. */

/* Note: g_sys is now defined in system_status.c and initialized by Sys_Init() */

/* UART receive buffers are now defined in ble.c and declared in ble.h */

/* Forward declarations for functions still in main.c */

// LED control function
/**
 * @brief 控制綠色 LED 開關
 * @param state ON 或 OFF
 */
/* Green LED control implemented in led.c (SetGreenLed). */

/**
 * @brief 設定 SPD 模式下的 GPIO 腳位為輸入
 */
/**
 * @brief 設定 SPD 模式下的 GPIO 腳位為輸入
 */
/* SPD-mode GPIO setup is provided by gpio.c if needed. */
/*---------------------------------------------------------------------------------------------------------*/
/*  Function for System Entry to Power Down Mode and Wake up source by GPIO Wake-up pin                    */
/*---------------------------------------------------------------------------------------------------------*/

#ifdef DPD_PC0 // for V2 Board
void WakeUpPinFunction(uint32_t u32PDMode, uint32_t u32EdgeType)
{
  DBG_PRINT("Enter to DPD Power-Down mode......\n");

  /* Check if all the debug messages are finished */
  while (!UART_IS_TX_EMPTY(UART0))
    ;
  while (!UART_IS_TX_EMPTY(UART1))
    ;

  /* Select Power-down mode */
  CLK_SetPowerDownMode(u32PDMode);

  /* Configure GPIO as Input mode */
  GPIO_SetMode(PC, BIT0, GPIO_MODE_INPUT);

  // Set Wake-up pin trigger type at Deep Power down mode
  CLK_EnableDPDWKPin(u32EdgeType);

  /* Enter to Power-down mode */
  CLK_PowerDown();

  /* Wait for Power-down mode wake-up reset happen */
  while (1)
    ;
}

#else // for SDP Mode

void WakeUpPinFunction(uint32_t u32PDMode)
{
  if ((SYS->CSERVER & SYS_CSERVER_VERSION_Msk) == 0x0)
    DBG_PRINT("Enter to SPD%d Power-Down mode......\n", (u32PDMode - 4));
  else
    DBG_PRINT("Enter to SPD Power-Down mode......\n");

  // Check if all the debug messages are finished
  while (!UART_IS_TX_EMPTY(UART0))
    ;
  while (!UART_IS_TX_EMPTY(UART1))
    ;
  SYS_UnlockReg();
  // Select Power-down mode
  CLK_SetPowerDownMode(u32PDMode);

  Led_SpdModeGpio();

  // GPIO SPD Power-down Wake-up Pin Select and Debounce Disable
  GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
  CLK_EnableSPDWKPin(1, 15, CLK_SPDWKPIN_FALLING, CLK_SPDWKPIN_DEBOUNCEDIS);

  // Enter to Power-down mode
  CLK_PowerDown();

  // Wait for Power-down mode wake-up reset happen
  while (1)
    ;
}
#endif

/* TMR0_IRQHandler is now in timer.c, which calls Led_TimerCallback() */

/* UART IRQ handler and BLE receive processing are implemented in ble.c */

/* BLE_UART_SEND is implemented in ble.c; main.c should use the prototype in ble.h */

/**
 * @brief BLE 指令傳送封裝，減少重複流程
 */
/* BLE helper functions implemented in ble.c */

/* GPIO interrupt handlers are implemented in gpio.c */

/* GPIO interrupt handlers are implemented in gpio.c */

/**
 * @brief 播放蜂鳴器
 * @param freq 頻率
 * @param time_ms 時間（毫秒）
 */
/* BuzzerPlay implemented in buzzer.c */

/**
 * @brief 讀取 Gsensor 三軸資料
 * @param axis 三軸資料儲存陣列
 */

void ReadGsensorAxis(int16_t *axis)
{
  uint8_t data_reg[6];
  int i;

  I2C_ReadMultiBytesOneReg(I2C0, Gsensor_Addr, 0x03, data_reg, 6);

  for (i = 0; i < 3; i++)
  {
    axis[i] = (int16_t)(data_reg[2 * i] << 8 | data_reg[2 * i + 1]) >> 4;
  }
}

/* G-sensor power control implemented in gsensor.c */

/**
 * @brief 處理 KeyA 事件，將 g_keyA_state 歸零
 */

void ProcessKeyAEvent(void)
{
  if (g_sys.keyA_state == 1)
  {
    // 可在此加入按鍵處理邏輯
  }
  g_sys.keyA_state = 0; // 已處理
}

void SYS_Init(void)
{
  /*---------------------------------------------------------------------------------------------------------*/
  /* Init System Clock                                                                                       */
  /*---------------------------------------------------------------------------------------------------------*/
  /* Core/system clock and board-level configuration
    -------------------------------------------------
    This function initializes the clocks and then delegates board-level
    pin/multi-function and PCLK configuration to board helper routines
    implemented in `gpio.c`. That centralization keeps main.c concise
    and avoids duplicate register writes across modules.

    NOTE: Caller (InitSystem) must unlock protected registers before
    calling SYS_Init(). Board I/O release is performed here to ensure
    the multi-function pin configuration operates on released I/O state.
  */

  /* Set XT1_OUT(PF.2) and XT1_IN(PF.3) to input mode */
  PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);

  /* Enable External XTAL (4~24 MHz) and wait for stability */
  CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
  CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

  /* Set core clock and HCLK source */
  CLK_SetCoreClock(PLL_CLOCK);
  CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_PLL, CLK_CLKDIV0_HCLK(1)); /* PLL @ 192MHz */

  /* Configure peripheral clocks common to board: PCLK divider and module clocks */
  Board_ConfigPCLKDiv(); /* sets CLK->PCLKDIV */
  CLK_EnableModuleClock(UART0_MODULE);
  CLK_EnableModuleClock(UART1_MODULE);
  CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HXT, CLK_CLKDIV0_UART0(1));
  CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_HXT, CLK_CLKDIV0_UART1(1));

  CLK_EnableModuleClock(TMR0_MODULE);
  CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_HXT, 0);

  CLK_EnableModuleClock(I2C0_MODULE);
  SystemCoreClockUpdate();

  /* Board-level multi-function pin configuration and I2C schmitt trigger */
  Board_ConfigMultiFuncPins();
  EnableI2C_Schmitt();

  /* Lock protected registers (if desired) is left to caller */
}

/* BLE helper functions (BLE_DISCONNECT, BLE_to_DLPS, BLEToRunMode,
   BLEDisconnect, BLESetName) are implemented in ble.c and declared in
   ble.h. Use those public APIs instead of local definitions. */

/**
 * @brief BLE傳送資料
 */
/* BLESendData and Gsensor power control functions are implemented in
   ble.c and gsensor.c respectively. Use those module APIs. */

/**
 * @brief 讀取Gsensor三軸資料
 */
/* GsensorReadAxis implemented in gsensor.c */

/**
 * @brief 播放蜂鳴器
 */

/* MCU_DLPS_GPIO implemented in buzzer.c */

/* SetGreenLedMode is now provided by led.h module */
/**
 * @brief 初始化 GPIO 腳位（LED、按鍵、Gsensor、Buzzer）
 */
void InitGpio(void)
{
  /* Use gpio module helpers to configure board pins. This centralizes
     pin configuration for buttons, G-sensor, I2C Schmitt trigger and
     multi-function pin setup. Specific modules handle LED/buzzer
     pin modes via their own Init functions. */
  Board_ConfigMultiFuncPins();
  EnableI2C_Schmitt();
  Init_Buttons_Gsensor();
}

/**
 * @brief 初始化 LED/Buzzer 狀態
 */
void InitLedBuzzer(void)
{
  SetGreenLedMode(2, 50);
  BuzzerPlay(1000, 100); // 2KHz ,50ms
}

/**
 * @brief BLE模組初始化流程
 */
/* BleSetup implemented in ble.c */

void InitSystem(void)
{
  /* Unlock protected registers */
  SYS_UnlockReg();
  /* Board I/O release is performed inside SYS_Init via Board_ReleaseIOPD().
     Call SYS_Init after unlocking registers so board helpers may update
     multi-function pins and clock dividers safely. */
  SYS_Init();
}

void InitPeripheral(void)
{
  Timer_Init(); /* Use new timer module */
  /* Initialize I2C and G-sensor via gsensor module */
  GsensorInit(100000);
  InitGpio();
  /* Open debug UART0 for retarget/printf */
  UART_Open(UART0, 115200);
  /* Initialize BLE transport (UART1) inside ble module */
  Ble_Init(115200);
  /* Initialize peripherals for LED and buzzer via their modules */
  Led_Init();
  Buzzer_Init();
  /* Configure LED default state */
  SetGreenLedMode(2, 50);
  Sys_Init();  /* Initialize system status */
  Game_Init(); /* Initialize game logic */
}

int main()
{
  int16_t axis[3];
  int32_t pre_time;
  uint8_t mac[5], device_name[5];

  InitSystem();
  InitPeripheral();

  DBG_PRINT("System Up\n");

// for make BLE setting NAME + MacAddr
#if 1
  delay_ms(200);
  BLE_UART_SEND(UART1, BLE_CMD_CCMD);
  delay_ms(200);
  CheckBleRecvMsg();
  BLE_UART_SEND(UART1, BLE_CMD_NAME_QUERY);
  delay_ms(20);
  CheckBleRecvMsg();

  memcpy(device_name, (const void *)&Sys_GetDeviceName()[12], 4);
  device_name[4] = '\0';
  DBG_PRINT("name = %s\r\n", device_name);

  if (strcmp((const void *)device_name, "ROPE\0") != 0)
  {
    DBG_PRINT("rename %s\n", device_name);
    BLE_UART_SEND(UART1, BLE_CMD_ADDR_QUERY);
    delay_ms(20);
    CheckBleRecvMsg();
    DBG_PRINT("addr = %s\n", Sys_GetMacAddr());
    memcpy(mac, (const void *)&Sys_GetMacAddr()[17], 4);
    mac[4] = '\0';
    DBG_PRINT("MAC address: %s \n", mac);
    BLE_UART_SEND(UART1, "AT+NAME=ROPE_%s\r\n", mac);
    delay_ms(500);
    CheckBleRecvMsg();
    BLE_UART_SEND(UART1, BLE_CMD_REBOOT);
    delay_ms(500);
    CheckBleRecvMsg();
  }
  BLE_UART_SEND(UART1, BLE_CMD_MODE_DATA);
  delay_ms(200);
  DBG_PRINT("rename OK\n");
#endif

#if enable_Gsensor_Mode
  MXC400_to_wakeup();
#endif
  while (1)
  {
    /* Process BLE messages */
    CheckBleRecvMsg();

    /* Main state machine using new game_logic module */
    switch (Sys_GetBleState())
    {
    case BLE_CONNECTED:
      switch (Sys_GetGameState())
      {
      case GAME_START:
        Game_ProcessRunning();
        break;
      case GAME_STOP:
        Game_ProcessIdle();
        break;
      }
      break;
    case BLE_DISCONNECTED:
      Game_ProcessDisconnected();
      break;
    }
  }
}

/* Game logic functions moved to game_logic.c */
/* ProcessGameStart -> Game_ProcessRunning */
/* ProcessBleDisconnected -> Game_ProcessDisconnected */

/*** (C) COPYRIGHT 2016 Richlink Technology Corp. ***/
