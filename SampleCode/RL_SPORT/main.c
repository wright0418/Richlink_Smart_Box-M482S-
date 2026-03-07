/*************************************************************************/
/*
 * @file     main.c
 * @version  V2.00
 * @brief    A project SPORT for M480 MCU.
 *
 * @copyright (C) 2023 Richlink  Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include "NuMicro.h"

/* Project configuration and common definitions */
#include "project_config.h"
#include "system_status.h"
#include "timer.h"
#include "game_logic.h"
#include "ble.h"
#include "led.h"
#include "buzzer.h"
#include "gpio.h"
#include "gsensor.h"
#include "board_test_gpio.h"
#include "adc.h"
#include "test_mode.h"
#include "usb_hid_mouse.h"
#include "power_mgmt.h"

#if USE_GSENSOR_JUMP_DETECT
#include "gsensor_jump_detect.h"
#endif

static volatile uint8_t g_usb_charge_mode = 0u;

/* Main-loop state (file-scope for readability) */
static uint32_t s_last_print_time = 0;
static uint32_t s_last_batt_check_time = 0;
static uint8_t s_low_batt = 0;
static uint8_t s_poweroff_done = 0;
static uint32_t s_last_usb_update = 0;

static void RL_HandleJumpDetect(uint32_t now, int16_t *axis)
{
#if USE_GSENSOR_JUMP_DETECT
  /* G-Sensor jump detection: poll at 50Hz (20ms interval) */
  static uint32_t last_gsensor_time = 0;

  if (get_elapsed_ms(last_gsensor_time) >= 20) /* 50Hz = 20ms */
  {
    last_gsensor_time = now;

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
#else
  (void)now;
  (void)axis;
#endif
}

static void RL_HandleGsensorPrint(uint32_t now, uint32_t *last_print_time, int16_t *axis)
{
  /* Periodically print G-sensor readings (every 200 ms) so user sees live data */
  if (get_elapsed_ms(*last_print_time) >= 200)
  {
    *last_print_time = now;
    GsensorReadAxis(axis);
#ifdef GSENSOR_MAIN_DEBUG
    DBG_PRINT("[Main] G-sensor XYZ = %d, %d, %d\n", axis[0], axis[1], axis[2]);
#endif
  }
}

static void RL_HandleBatteryCheck(uint32_t now, uint32_t *last_batt_check_time, uint8_t *low_batt)
{
  if (get_elapsed_ms(*last_batt_check_time) >= LOW_BATT_CHECK_INTERVAL_MS)
  {
    *last_batt_check_time = now;
    uint16_t raw = Adc_ReadBatteryRawAvg(ADC_BATT_AVG_SAMPLES);
    float vbat = Adc_ConvertRawToBatteryV(raw);
    uint8_t is_low = (vbat <= ADC_BATT_LOW_V);
    if (is_low && !(*low_batt))
    {
      DBG_PRINT("[Main] Low battery: %.2fV\n", vbat);
    }
    *low_batt = is_low;
  }
}

static void RL_UpdateLedState(uint8_t low_batt)
{
  if (low_batt)
  {
    SetGreenLedMode(LOW_BATT_LED_FREQ_HZ, LOW_BATT_LED_DUTY);
    return;
  }

  if (Sys_GetGameState() == GAME_START)
  {
    SetGreenLedMode(1.0f, 0.1f);
    return;
  }

  if (Sys_GetBleState() == BLE_CONNECTED && Sys_GetGameState() == GAME_STOP)
  {
    SetGreenLedMode(0.5f, 0.5f);
    return;
  }

#if USE_GSENSOR_JUMP_DETECT
  if (JumpDetect_IsCalibrating())
  {
    return;
  }

  if (JumpDetect_IsPreCalibDone())
  {
    SetGreenLedMode(GS_CAL_LED_CAL_FREQ_HZ, GS_CAL_LED_DUTY);
  }
  else
  {
    SetGreenLedMode(GS_CAL_LED_UNCAL_FREQ_HZ, GS_CAL_LED_DUTY);
  }
#else
  SetGreenLedMode(0.5f, 0.5f);
#endif
}

static void RL_HandleBleAndGameState(void)
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

static void RL_HandleIdlePowerOff(uint8_t *poweroff_done)
{
  /* Idle timeout -> power off (PA11 low) */
  if (!(*poweroff_done) && Sys_GetIdleState())
  {
    *poweroff_done = 1u;
    DBG_PRINT("[Main] Idle timeout, power off\n");
    DBG_PRINT("[Main] BLE disconnect before power off\n");
    BLEDisconnect();
    delay_ms(50);
    SetGreenLedMode(0, 0);
    PowerLock_Set(0);
    while (1)
    {
      /* wait for power to cut */
    }
  }
}

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

/*---------------------------------------------------------------------------------------------------------*/
/*  Function for System Entry to Power Down Mode and Wake up source by GPIO Wake-up pin                    */
/*---------------------------------------------------------------------------------------------------------*/

/**
 * @brief Handle KeyA event: clear g_keyA_state
 */

void ProcessKeyAEvent(void)
{
  /* Key press handling: do not trigger G-sensor calibration here. */
  /* Reset movement inactivity timer on any user key activity */
  Game_ResetMovementTimer();
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
    calling SYS_Init(). Board I/O release is performed here (via
    Board_ReleaseIOPD) to ensure the multi-function pin configuration
    operates on released I/O state.
  */

  /* Release I/O hold status (SPD/low power) before touching pin MFP */
  Board_ReleaseIOPD();

  /* Set XT1_OUT(PF.2) and XT1_IN(PF.3) to input mode */
  PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);

  /* Enable External XTAL (4~24 MHz) and wait for stability */
  CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
  CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

  /*
   * Power profile:
   * - PLL: 96 MHz (project_config.h: PLL_CLOCK)
   * - CPU/HCLK: 48 MHz (PLL / 2)
   * - USB clock is configured in usb_hid_mouse.c to 48 MHz from PLL divider
   */
  CLK_SetCoreClock(PLL_CLOCK);
  CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_PLL, CLK_CLKDIV0_HCLK(2)); /* HCLK = PLL / 2 = 48MHz */

  /* Configure peripheral clocks common to board: PCLK divider and module clocks */
  Board_ConfigPCLKDiv(); /* sets CLK->PCLKDIV */
  CLK_EnableModuleClock(UART0_MODULE);
  CLK_EnableModuleClock(UART1_MODULE);
  CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HXT, CLK_CLKDIV0_UART0(1));
  CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_HXT, CLK_CLKDIV0_UART1(1));

  CLK_EnableModuleClock(TMR0_MODULE);
  CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_HXT, 0);

  CLK_EnableModuleClock(I2C0_MODULE);
  CLK_EnableModuleClock(EADC_MODULE);
  CLK_SetModuleClock(EADC_MODULE, 0, CLK_CLKDIV0_EADC(8));
  SystemCoreClockUpdate();

  /* Board-level multi-function pin configuration and I2C schmitt trigger */
  Board_ConfigMultiFuncPins();
  EnableI2C_Schmitt();

  /* Lock protected registers (if desired) is left to caller */
}

/*
 * Initialization orchestration
 * ----------------------------
 * Keep initialization ordered and centralized, so we don't scatter clock/pin
 * writes across unrelated modules.
 *
 * Order (high-level):
 *  1) System core (unlock -> clocks -> pin mux -> lock)
 *  2) Board GPIO/interrupt inputs (buttons, sensor IRQ)
 *  3) Drivers/peripherals (timer, UART, BLE, sensor, LED/buzzer)
 *  4) App logic (system state, game, optional jump detect)
 */

static void RL_InitSystemCore(void)
{
  SYS_UnlockReg();
  SYS_Init();

  g_usb_charge_mode = PowerMgmt_DetectUsbCharge();
  if (!g_usb_charge_mode)
  {
    PowerLock_Init();
  }
  // SYS_LockReg();
}

static void RL_InitBoardInputs(void)
{
  /* Button, HALL (optional) and G-sensor interrupt input pins + NVIC */
  Gpio_Init();
}

static void RL_InitDrivers(void)
{
  /* Time base */
  Timer_Init();

  /* Sensors */
  Gsensor_Init(100000, FSR_2G);
  GsensorWakeup();

  /* Battery ADC */
  Adc_InitBattery();

  /* Debug/printf UART (retarget uses UART0) */
  UART_Open(UART0, 115200);

  /* BLE transport (UART1) */
  Ble_Init(115200);

  /* UI outputs */
  Led_Init();
  Buzzer_Init();
  SetGreenLedMode(2, 50);
}

static void RL_InitApplication(void)
{
  Sys_Init();
  Game_Init();

#if USE_GSENSOR_JUMP_DETECT
  JumpDetect_Init();
  DBG_PRINT("[Main] G-Sensor jump detection mode enabled\n");
  DBG_PRINT("[Main] Auto calibration will start when stable\n");
#else
  DBG_PRINT("[Main] HALL Sensor jump detection mode enabled\n");
#endif
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

int main()
{
  int16_t axis[3];
  uint8_t mac[5], device_name[5];

  RL_InitSystemCore();
  RL_InitBoardInputs();

  if (g_usb_charge_mode)
  {
    // charge mode LED on and run charge loop (blocking until power cut)
    GPIO_SetMode(PB, BIT3, GPIO_MODE_OUTPUT);
    PB->DOUT |= BIT3;
    PowerMgmt_RunChargeLoop();
  }

  RL_InitDrivers();
  RL_InitApplication();

#if USE_GSENSOR_JUMP_DETECT
  JumpDetect_InitPreCalib(get_ticks_ms());
#endif

  /* Run board GPIO test once at startup */
  BoardTest_RunAll();

  DBG_PRINT("System Up\n");
  BLEToRunMode();
// for make BLE setting NAME + MacAddr
#if 1
  Ble_RenameFlow(device_name, mac);
#endif

  while (1)
  {
    uint32_t now = get_ticks_ms();

    TestMode_PollEnter();
    TestMode_RunMenuIfActive();

    if (UsbHidMouse_TestIsActive())
    {
      if (get_elapsed_ms(s_last_usb_update) >= 1u)
      {
        s_last_usb_update = now;
        UsbHidMouse_TestUpdate();
      }
      continue;
    }

#if USE_GSENSOR_JUMP_DETECT
    JumpDetect_TryAutoCalibration(now);
#endif
    RL_HandleJumpDetect(now, axis);
#if USE_GSENSOR_JUMP_DETECT
    JumpDetect_UpdatePreCalibState();
#endif
    RL_HandleGsensorPrint(now, &s_last_print_time, axis);
    RL_HandleBatteryCheck(now, &s_last_batt_check_time, &s_low_batt);
    /* Hall sensor IRQ print (main loop, not ISR) */
    if (Sys_GetHallPb7IrqFlag())
    {
      uint16_t total = Sys_GetJumpTimes();
      Sys_SetHallPb7IrqFlag(0);
      DBG_PRINT("[HALL] PB7 IRQ total=%u\n", (unsigned)total);
    }
    /* Process button events */
    if (Sys_GetKeyAFlag())
    {
      Sys_SetKeyAFlag(0);
      ProcessKeyAEvent();
    }

    RL_HandleBleAndGameState();
    RL_HandleIdlePowerOff(&s_poweroff_done);
    RL_UpdateLedState(s_low_batt);
  }
}

/*** (C) COPYRIGHT 2016 Richlink Technology Corp. ***/
