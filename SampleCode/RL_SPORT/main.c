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
#include "drivers/timer.h"
#include "app/game_logic.h"
#include "ble.h"
#include "drivers/led.h"
#include "drivers/buzzer.h"
#include "board/gpio.h"
#include "drivers/gsensor.h"
#include "board/board_test_gpio.h"
#include "drivers/adc.h"
#include "test_mode.h"
#include "board/usb_hid/usb_hid_mouse.h"
#include "board/power_mgmt.h"
#include "ble_at_repl.h"
#if USE_MOLE_GAME
#include "app/mole_game.h"
#endif

#if USE_GSENSOR_JUMP_DETECT
#include "app/algorithms/gsensor_jump_detect.h"
#endif
#if USE_HALL_ANTICHEAT
#include "app/algorithms/hall_anticheat.h"
#endif

static volatile uint8_t g_usb_charge_mode = 0u;

/* Main-loop state (file-scope for readability) */
static uint32_t s_last_print_time = 0;
static uint32_t s_last_batt_check_time = 0;
static uint8_t s_low_batt = 0;
static uint8_t s_poweroff_done = 0;
static uint32_t s_last_usb_update = 0;
static uint8_t s_ble_rename_started = 0u;
static uint8_t s_ble_rename_done = 0u;
static uint8_t s_charge_mode_initialized = 0u;
static uint8_t s_hall_edge_residual = 0u;
#define RL_IDLE_LED_FREQ_HZ (1.0f / 3.0f)
#define RL_IDLE_LED_DUTY (0.1f)
#if MOLE_TEST_TRACE_ENABLE
#define MOLE_MAIN_TRACE(fmt, ...) printf("[MOLE_TEST] " fmt, ##__VA_ARGS__)
#else
#define MOLE_MAIN_TRACE(fmt, ...)
#endif
#if USE_MOLE_GAME && MOLE_LOW_BATT_POWER_OFF
static uint8_t s_low_batt_shutdown_count = 0u;
static uint32_t s_low_batt_shutdown_last_check = 0u;
#endif
#if USE_HALL_ANTICHEAT
static uint16_t s_hall_raw_total = 0u;
static uint8_t s_prev_game_running = 0u;
#endif

static void RL_StartupBeep(void)
{
  /* One short beep at boot, independent from board test flow. */
  BuzzerPlay(1200u, 80u);
  delay_ms(120u);
}

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
#elif USE_HALL_ANTICHEAT
  /* Anti-cheat: poll G-sensor at 50Hz for parallel jump estimation */
  {
    static uint32_t last_ac_time = 0;
    if (get_elapsed_ms(last_ac_time) >= 20) /* 50Hz */
    {
      last_ac_time = now;
      GsensorReadAxis(axis);
      HallAntiCheat_Process(axis);
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
    Adc_UpdateVdda();
    float vdda = Adc_GetVdda();
    uint8_t is_low = Adc_IsVddaLow();
    DBG_PRINT("[BATT] vdda=%.3fV %s\n", (double)vdda, is_low ? "LOW" : "OK");
    *low_batt = is_low;
  }
}

static void RL_UpdateLedState(uint8_t low_batt)
{
  /* Low battery LED takes priority over all other LED states */
  if (low_batt)
  {
#if USE_MOLE_GAME
    /* Mole profile: avoid fast low-battery blink (often false-positive during bring-up).
       Keep a stable heartbeat cadence for field testing. */
    SetGreenLedMode(RL_IDLE_LED_FREQ_HZ, RL_IDLE_LED_DUTY);
#else
    SetGreenLedMode(LOW_BATT_LED_FREQ_HZ, LOW_BATT_LED_DUTY);
#endif
    return;
  }

  /* In REPL mode or when LED is under REPL override, skip entirely */
  if (Sys_GetReplMode() || Sys_GetLedOverride())
  {
    return;
  }

#if USE_MOLE_GAME
  /* Mole profile keeps PB3 as a constant heartbeat indicator. */
  SetGreenLedMode(RL_IDLE_LED_FREQ_HZ, RL_IDLE_LED_DUTY);
  return;
#endif

  if (Sys_GetGameState() == GAME_START)
  {
    SetGreenLedMode(1.0f, 0.1f);
    return;
  }

  if (Sys_GetBleState() == BLE_CONNECTED && Sys_GetGameState() == GAME_STOP)
  {
    SetGreenLedMode(RL_IDLE_LED_FREQ_HZ, RL_IDLE_LED_DUTY);
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
  SetGreenLedMode(RL_IDLE_LED_FREQ_HZ, RL_IDLE_LED_DUTY);
#endif
}

static void RL_HandleBleAndGameState(void)
{
  /* Process BLE messages */
  CheckBleRecvMsg();

  if (BleAtRepl_IsActive())
  {
    return;
  }

#if USE_MOLE_GAME
  MoleGame_Process(get_ticks_ms());
  return;
#else

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
#endif
}

static void RL_HandleIdlePowerOff(uint8_t *poweroff_done)
{
#if USE_MOLE_GAME && MOLE_DISABLE_IDLE_POWER_OFF
  (void)poweroff_done;
  return;
#else
  /* Idle timeout -> power off (PA11 low) */
  if (!(*poweroff_done) && Sys_GetIdleState())
  {
    *poweroff_done = 1u;
    DBG_PRINT("[Main] Idle timeout, power off\n");
    DBG_PRINT("[Main] Power off directly (skip BLE DISC command)\n");
    SetGreenLedMode(0, 0);
    PowerLock_Set(0);
    while (1)
    {
      /* wait for power to cut */
    }
  }
#endif
}

static void RL_HandleMoleLowBatteryShutdown(uint8_t low_batt)
{
#if USE_MOLE_GAME && MOLE_LOW_BATT_POWER_OFF
  uint32_t now = get_ticks_ms();

  /* Keep low-battery protection for battery operation, but do not force
     shutdown while USB power is present (common during bring-up/debug).
     Otherwise the firmware can issue AT+DISC and then park in while(1),
     which looks like BLE connected-message loss on UART logs. */
  if (g_usb_charge_mode)
  {
    if (low_batt)
    {
      MOLE_MAIN_TRACE("LOW_BATT detected but USB mode active, skip shutdown\r\n");
    }
    s_low_batt_shutdown_count = 0u;
    s_low_batt_shutdown_last_check = now;
    return;
  }

  if (!low_batt)
  {
    s_low_batt_shutdown_count = 0u;
    s_low_batt_shutdown_last_check = now;
    return;
  }

  /* Do not shut down before BLE has a chance to connect.
     Your logs showed repeated LOW_BATT confirms causing early shutdown
     before CONNECTED message appears. Keep warning behavior, but defer
     hard power-off until BLE is connected. */
  if (Sys_GetBleState() != BLE_CONNECTED)
  {
    MOLE_MAIN_TRACE("LOW_BATT deferred, BLE state=%u (need connected)\r\n",
                    (unsigned)Sys_GetBleState());
    s_low_batt_shutdown_count = 0u;
    s_low_batt_shutdown_last_check = now;
    return;
  }

  if (get_elapsed_ms(s_low_batt_shutdown_last_check) < LOW_BATT_CHECK_INTERVAL_MS)
  {
    return;
  }

  s_low_batt_shutdown_last_check = now;
  if (s_low_batt_shutdown_count < MOLE_LOW_BATT_SHUTDOWN_CONFIRM_COUNT)
  {
    s_low_batt_shutdown_count++;
    MOLE_MAIN_TRACE("LOW_BATT confirm %u/%u\r\n",
                    (unsigned)s_low_batt_shutdown_count,
                    (unsigned)MOLE_LOW_BATT_SHUTDOWN_CONFIRM_COUNT);
  }

  if (s_low_batt_shutdown_count >= MOLE_LOW_BATT_SHUTDOWN_CONFIRM_COUNT)
  {
    MOLE_MAIN_TRACE("LOW_BATT confirmed, shutdown flow start\r\n");
    MoleGame_ShutdownOutputs();
    MOLE_MAIN_TRACE("LOW_BATT shutdown: power cut directly (skip AT+DISC)\r\n");
    delay_ms(MOLE_LOW_BATT_SHUTDOWN_GRACE_MS);
    PowerLock_Set(0);
    while (1)
    {
      /* wait for power to cut */
    }
  }
#else
  (void)low_batt;
#endif
}

/*
 * Module implementation notes:
 * - BLE transport/UART IRQ are implemented in ble.c; pure text parsing helpers
 *   (e.g. BleParser_ParseCommand) are split into protocol/ble_parser.c/.h for unit tests.
 * - System status storage and accessor APIs live in system_status.c and are initialized by Sys_Init()
 * - GPIO interrupts, buttons and board-level pin config are in board/gpio.c
 * - LED, Buzzer, Timer and G-sensor drivers live in led.c, buzzer.c, timer.c and gsensor.c
 *
 * Rationale: keep main.c focused on initialization and high-level flow; use module public APIs
 * (see respective headers: ble.h, board/gpio.h, led.h, gsensor.h).
 */

/*---------------------------------------------------------------------------------------------------------*/
/*  Function for System Entry to Power Down Mode and Wake up source by GPIO Wake-up pin                    */
/*---------------------------------------------------------------------------------------------------------*/

/**
 * @brief Handle user KeyA press events.
 *
 * The application uses this event to refresh the movement inactivity timer
 * without starting sensor calibration or other game-specific flows.
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
#if USE_MOLE_GAME && MOLE_DISABLE_USB_CHARGE_LOOP
  PowerLock_Init();
#else
  if (!g_usb_charge_mode)
  {
    PowerLock_Init();
  }
#endif
  // SYS_LockReg();
}

static void RL_InitBoardInputs(void)
{
  /* Button, HALL (optional) and G-sensor interrupt input pins + NVIC */
  Gpio_Init();
}

static void RL_LogResetSource(void)
{
  uint32_t rst = SYS->RSTSTS;

  DBG_PRINT("[RST] RSTSTS=0x%08lX POR=%u PIN=%u WDT=%u LVR=%u BOD=%u SYS=%u CPU=%u CPULK=%u\n",
            (unsigned long)rst,
            (unsigned)((rst & SYS_RSTSTS_PORF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_PINRF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_WDTRF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_LVRF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_BODRF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_SYSRF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_CPURF_Msk) != 0u),
            (unsigned)((rst & SYS_RSTSTS_CPULKRF_Msk) != 0u));

  SYS_CLEAR_RST_SOURCE(rst);
}

static void RL_InitDrivers(void)
{
  /* Time base */
  Timer_Init();

  /* Debug/printf UART (retarget uses UART0) */
  UART_Open(UART0, 115200);
  RL_LogResetSource();

  /* Sensors */
  Gsensor_Init(GSENSOR_I2C_BUS_HZ, FSR_2G);
  DBG_PRINT("[GSENSOR] backend=%s addr=0x%02X\n",
            GsensorGetDeviceName(),
            (unsigned)GsensorGetI2CAddress());

  /* Battery ADC */
  Adc_Init();

  /* BLE transport (UART1) */
  Ble_Init(115200);

  /* UI outputs */
  Led_Init();
  Buzzer_Init();
  /* Default boot LED: every 3 seconds on for 0.3s (freq=1/3 Hz, duty=10%) */
  SetGreenLedMode(RL_IDLE_LED_FREQ_HZ, RL_IDLE_LED_DUTY);
}

static void RL_InitApplication(void)
{
  Sys_Init();
  BleAtRepl_Init();
  Game_Init();

#if USE_MOLE_GAME
  MoleGame_Init();
  DBG_PRINT("[Main] Mole game firmware profile enabled\n");
#endif

#if USE_GSENSOR_JUMP_DETECT
  JumpDetect_Init();
  DBG_PRINT("[Main] G-Sensor jump detection mode enabled\n");
  DBG_PRINT("[Main] Auto calibration will start when stable\n");
#elif USE_HALL_ANTICHEAT
  HallAntiCheat_Init();
  DBG_PRINT("[Main] HALL + anti-cheat (G-sensor validation) enabled\n");
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

  RL_InitSystemCore();
  RL_InitBoardInputs();

  /* If USB charge detected at boot, enter charge-mode loop before
     initializing drivers or starting the game. In charge mode we keep
     the power-lock low .
     This prevents the game from starting while on USB power. */
#if !(USE_MOLE_GAME && MOLE_DISABLE_USB_CHARGE_LOOP)
  if (g_usb_charge_mode)
  {
    DBG_PRINT("[Main] USB charge detected at boot, entering charge mode\n");
    /* Set power lock low so board will not stay powered when USB removed */
    PowerMgmt_ChargeModeInit();
    /* This will block here until USB is removed and charge loop exits */
    PowerMgmt_RunChargeLoop();
  }
#else
  if (g_usb_charge_mode)
  {
    DBG_PRINT("[Main] USB charge detected; Mole profile skips charge-mode loop\n");
  }
#endif

  RL_InitDrivers();
  RL_StartupBeep();
  RL_InitApplication();

#if USE_GSENSOR_JUMP_DETECT
  JumpDetect_InitPreCalib(get_ticks_ms());
#endif

  /* Optional board test at boot (for fast board check stage). */
#if BOARD_TEST_AUTORUN
  BoardTest_RunAll();
#else
  DBG_PRINT("[Main] Board test autorun disabled\n");
#endif

  DBG_PRINT("System Up\n");
  BLEToRunMode();
  Ble_RenameFlowStart();
  s_ble_rename_started = 1u;

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

    if (s_ble_rename_started && !s_ble_rename_done)
    {
      Ble_RenameFlowProcess();
      s_ble_rename_done = Ble_RenameFlowIsDone();
    }

    BleAtRepl_RunIfActive();

    /* Battery check runs unconditionally (including REPL mode) */
    RL_HandleBatteryCheck(now, &s_last_batt_check_time, &s_low_batt);

    /* Low battery LED must show even in REPL mode */
    RL_UpdateLedState(s_low_batt);

    /* ---- REPL mode handling ----
       In MOLE profile, keep raw BLE packet path alive even if REPL/LED override
       is set, otherwise BLE binary frames can be received on UART1 but never
       parsed/applied to WS2812 output. */
    if (Sys_GetReplMode() || Sys_GetLedOverride())
    {
      CheckBleRecvMsg();
#if USE_MOLE_GAME
      MoleGame_Process(now);
#endif
      continue;
    }

#if USE_MOLE_GAME
    if (Sys_GetKeyAFlag())
    {
      Sys_SetKeyAFlag(0);
      MoleGame_OnButtonEvent(now);
    }

    RL_HandleBleAndGameState();
    RL_HandleMoleLowBatteryShutdown(s_low_batt);
    continue;
#endif

#if USE_GSENSOR_JUMP_DETECT
    JumpDetect_TryAutoCalibration(now);
#endif
    RL_HandleJumpDetect(now, axis);
#if USE_GSENSOR_JUMP_DETECT
    JumpDetect_UpdatePreCalibState();
#endif
    RL_HandleGsensorPrint(now, &s_last_print_time, axis);
#if USE_HALL_ANTICHEAT
    /* Detect game-start transition and reset anti-cheat state */
    {
      uint8_t game_running = (Sys_GetGameState() == GAME_START) ? 1u : 0u;
      if (game_running && !s_prev_game_running)
      {
        s_hall_raw_total = 0u;
        HallAntiCheat_Reset();
        DBG_PRINT("[AntiCheat] Game started - counters reset\n");
      }
      s_prev_game_running = game_running;
    }
#endif
    /* Hall sensor IRQ print (main loop, not ISR) */
    uint8_t edges = 0u;
    if (Sys_GetHallPb7IrqFlag())
    {
      edges = Sys_TakeHallPb7PendingEdges();

      if (Sys_GetGameState() == GAME_START)
      {
        uint16_t jumps = GameAlgo_CalcJumpsFromEdges(s_hall_edge_residual, edges, &s_hall_edge_residual);
        if (jumps > 0u)
        {
#if USE_HALL_ANTICHEAT
          s_hall_raw_total += jumps;
          uint16_t validated = HallAntiCheat_ValidateHallTotal(s_hall_raw_total);
          uint16_t current = Sys_GetJumpTimes();
          if (validated > current)
          {
            Sys_AddJumpTimes(validated - current);
          }
#else
          Sys_AddJumpTimes(jumps);
#endif
        }
      }
      else
      {
        /* keep only counter reset here; LED mode is managed centrally in RL_UpdateLedState */
      }
      s_hall_edge_residual = 0u;
#if USE_HALL_ANTICHEAT
      s_hall_raw_total = 0u;
#endif
    }

    uint16_t total = Sys_GetJumpTimes();
    Sys_SetHallPb7IrqFlag(0);
    DBG_PRINT("[HALL] PB7 edges=%u total=%u\n", (unsigned)edges, (unsigned)total);
    /* Process button events */
    if (Sys_GetKeyAFlag())
    {
      Sys_SetKeyAFlag(0);
      ProcessKeyAEvent();
    }

    RL_HandleBleAndGameState();
    RL_HandleIdlePowerOff(&s_poweroff_done);
    /* LED state (non-REPL path): low-batt already handled above, this covers game/BLE states */
    RL_UpdateLedState(s_low_batt);
  }
}

/*** (C) COPYRIGHT 2016 Richlink Technology Corp. ***/
