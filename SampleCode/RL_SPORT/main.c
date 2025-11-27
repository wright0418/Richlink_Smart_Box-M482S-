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

// GPIO Pins (move to gpio.h)
#define PIN_KEYA PB15
#define PIN_JUMP1 PB7
#define PIN_JUMP2 PB8
#define PIN_GSENSOR_INT PC5

// BLE Command Strings (already defined in ble.h)
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

// BLE command enum (implemented in ble.c)
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
/*
 * Module implementation notes:
 * - BLE (BLEParseCommand, UART IRQ/RX handling, BLE transport) implemented in ble.c, declared in ble.h
 * - System status and global `g_sys` live in system_status.c and are initialized by Sys_Init()
 * - GPIO interrupts, buttons and board-level pin config are in gpio.c
 * - LED, Buzzer, Timer and G-sensor drivers live in led.c, buzzer.c, timer.c and gsensor.c
 *
 * Rationale: keep main.c focused on initialization and high-level flow; use module public APIs
 * (see respective headers: ble.h, gpio.h, led.h, gsensor.h).
 */

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

/**
 * @brief Handle KeyA event: clear g_keyA_state
 */

void ProcessKeyAEvent(void)
{
  if (g_sys.keyA_state == 1)
  {
    // Optional: add key handling logic here
  }
  g_sys.keyA_state = 0; // processed
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

/*
 * Module API summary:
 * - Use the helper BLE APIs in ble.h/ble.c for BLE-related helpers
 *   (e.g. BLE_DISCONNECT, BLE_to_DLPS, BLEToRunMode) and UART transport.
 * - Use gsensor.h/gsensor.c for G-sensor reads and power management.
 * - Use buzzer.h/buzzer.c for buzzer and MCU DLPS-related GPIO and helpers.
 * - Use led.h/led.c for LED configuration (e.g. SetGreenLedMode).
 *
 * Avoid duplicating low-level hardware behavior in main.c; call module
 * public APIs instead.
 */

/**
 * @brief Initialize GPIO pins (LED, buttons, G-sensor, Buzzer)
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
 * @brief Initialize LED and Buzzer state
 */
void InitLedBuzzer(void)
{
  SetGreenLedMode(2, 50);
  BuzzerPlay(1000, 100); // 2KHz ,50ms
}

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
  /* Initialize G-sensor: I2C 100kHz and default full-scale range 2G */
  GsensorInit(100000, FSR_2G);
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
  /* Ensure the sensor is awake (uses configured fsr) */
  GsensorWakeup();

#endif
  while (1)
  {

#if enable_Gsensor_Mode
    /* Read G-sensor axis data if in G-sensor mode */
    GsensorReadAxis(axis);
    DBG_PRINT("X,%d ,Y,%d ,Z,%d\n", axis[0], axis[1], axis[2]);
#endif
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

/*** (C) COPYRIGHT 2016 Richlink Technology Corp. ***/
