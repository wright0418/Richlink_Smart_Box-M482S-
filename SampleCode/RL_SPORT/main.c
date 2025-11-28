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

#if USE_GSENSOR_JUMP_DETECT
#include "gsensor_jump_detect.h"
#endif

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

/**
 * @brief Handle KeyA event: clear g_keyA_state
 */

void ProcessKeyAEvent(void)
{
#if USE_GSENSOR_JUMP_DETECT
  /* Start G-Sensor calibration when button pressed */
  /* No game state restriction - allow calibration anytime when not already calibrating */
  if (!JumpDetect_IsCalibrating())
  {
    DBG_PRINT("[Main] Starting G-Sensor calibration...\n");
    JumpDetect_StartCalibration();
  }
  else
  {
    DBG_PRINT("[Main] Calibration already in progress\n");
  }
#else
  // Optional: add key handling logic here for HALL sensor mode
#endif
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

#if USE_GSENSOR_JUMP_DETECT
  /* Initialize G-Sensor jump detection module */
  JumpDetect_Init();
  DBG_PRINT("[Main] G-Sensor jump detection mode enabled\n");
  DBG_PRINT("[Main] Press button (PB15) to start calibration\n");
#else
  DBG_PRINT("[Main] HALL Sensor jump detection mode enabled\n");
#endif
}

int main()
{
  int16_t axis[3];
  int32_t pre_time;
  uint8_t mac[5], device_name[5];

  InitSystem();
  /* Handle potential SPD wake (release IO hold, clear flags, restore button interrupt) */
  PowerMgmt_HandleWake();
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
#if USE_GSENSOR_JUMP_DETECT
    /* G-Sensor jump detection: poll at 50Hz (20ms interval) */
    static uint32_t last_gsensor_time = 0;
    uint32_t now = get_ticks_ms();

    if (get_elapsed_ms(last_gsensor_time) >= 20) /* 50Hz = 20ms */
    {
      last_gsensor_time = now;

      int16_t axis[3];
      GsensorReadAxis(axis);

      /* Process calibration or jump detection */
      if (JumpDetect_IsCalibrating())
      {
        JumpDetect_ProcessCalibration(axis);
      }
      else if (JumpDetect_IsReady())
      {
        JumpDetect_Process(axis);
      }
    }

#endif
    /* Process button events */
    if (Sys_GetKeyAFlag())
    {
      Sys_SetKeyAFlag(0);
      ProcessKeyAEvent();
    }

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
