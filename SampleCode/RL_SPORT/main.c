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
#include <math.h>
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
#include "i2c.h"
#include "board_test_gpio.h"
#include "adc.h"
#include "test_mode.h"
#include "usb_hid_mouse.h"

#if USE_GSENSOR_JUMP_DETECT
#include "gsensor_jump_detect.h"
#endif

static volatile uint8_t g_usb_charge_mode = 0u;

#if USE_GSENSOR_JUMP_DETECT
static uint8_t g_gs_calib_started = 0u;
static uint8_t g_gs_calib_done = 0u;
static uint32_t g_gs_calib_schedule_at = 0u;
static uint32_t g_gs_calib_stable_since = 0u;
static uint32_t g_gs_calib_last_sample_time = 0u;
static float g_gs_calib_window[GS_CAL_STABLE_WINDOW_SAMPLES];
static uint8_t g_gs_calib_window_idx = 0u;
static uint8_t g_gs_calib_window_count = 0u;
static uint8_t g_gs_calib_stable_now = 0u;
#endif

static uint8_t RL_DetectUsbChargeMode(void)
{
  USBDetect_Init();
  CLK_SysTickDelay(50000); /* 50 ms debounce */
  return USBDetect_IsHigh();
}

static void RL_RunUsbChargeLoop(void)
{
  while (1)
  {
    /* USB charging auto-boot mode: no power lock, no game */
  }
}

static uint8_t RL_IsHexChar(char c)
{
  return (uint8_t)((c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'F') ||
                   (c >= 'a' && c <= 'f'));
}

static uint8_t RL_ExtractMacSuffix4(const char *src, char *out4)
{
  if (!src || !out4)
  {
    return 0u;
  }

  char hex_buf[16];
  uint8_t hex_len = 0u;
  for (size_t i = 0u; src[i] != '\0'; i++)
  {
    if (RL_IsHexChar(src[i]))
    {
      if (hex_len < (uint8_t)sizeof(hex_buf))
      {
        hex_buf[hex_len++] = src[i];
      }
    }
  }

  if (hex_len < 4u)
  {
    out4[0] = '\0';
    return 0u;
  }

  out4[0] = hex_buf[hex_len - 4u];
  out4[1] = hex_buf[hex_len - 3u];
  out4[2] = hex_buf[hex_len - 2u];
  out4[3] = hex_buf[hex_len - 1u];
  out4[4] = '\0';
  return 1u;
}

static uint8_t RL_HasValidRopeSuffix(const char *name)
{
  if (!name)
  {
    return 0u;
  }

  const char *p = strstr(name, "ROPE_");
  if (!p)
  {
    return 0u;
  }

  p += 5; /* skip "ROPE_" */
  for (uint8_t i = 0u; i < 4u; i++)
  {
    if (!RL_IsHexChar(p[i]))
    {
      return 0u;
    }
  }
  return 1u;
}

static void RL_BleRenameFlow(uint8_t *device_name, uint8_t *mac)
{
  delay_ms(200);
  BLE_UART_SEND(UART1, BLE_CMD_CCMD);
  delay_ms(200);
  CheckBleRecvMsg();
  BLE_UART_SEND(UART1, BLE_CMD_NAME_QUERY);
  delay_ms(20);
  CheckBleRecvMsg();

  DBG_PRINT("[BLE] name raw: '%s'\r\n", Sys_GetDeviceName());

  memcpy(device_name, (const void *)&Sys_GetDeviceName()[12], 4);
  device_name[4] = '\0';
  DBG_PRINT("name = %s\r\n", device_name);

  uint8_t has_valid_rope = RL_HasValidRopeSuffix(Sys_GetDeviceName());
  DBG_PRINT("[BLE] rope suffix valid: %u\r\n", has_valid_rope);

  if (!has_valid_rope)
  {
    DBG_PRINT("rename %s\n", device_name);
    BLE_UART_SEND(UART1, BLE_CMD_ADDR_QUERY);
    for (uint8_t retry = 0u; retry < 10u; retry++)
    {
      delay_ms(20);
      CheckBleRecvMsg();
      if (strlen((const char *)Sys_GetMacAddr()) > 0u)
      {
        break;
      }
    }
    DBG_PRINT("[BLE] addr raw: '%s'\n", Sys_GetMacAddr());

    char mac_suffix[5];
    uint8_t has_suffix = RL_ExtractMacSuffix4(Sys_GetMacAddr(), mac_suffix);
    if (!has_suffix && strlen((const char *)Sys_GetMacAddr()) >= 21u)
    {
      memcpy(mac_suffix, (const void *)&Sys_GetMacAddr()[17], 4);
      mac_suffix[4] = '\0';
      has_suffix = 1u;
    }

    DBG_PRINT("[BLE] mac suffix: '%s' (valid=%u)\n", mac_suffix, has_suffix);

    if (has_suffix)
    {
      memcpy(mac, mac_suffix, 4);
      mac[4] = '\0';
      DBG_PRINT("MAC address: %s \n", mac);
      BLE_UART_SEND(UART1, "AT+NAME=ROPE_%s\r\n", mac);
    }
    else
    {
      DBG_PRINT("[BLE] WARN: MAC suffix not found, skip rename\n");
    }
    delay_ms(500);
    CheckBleRecvMsg();
    BLE_UART_SEND(UART1, BLE_CMD_REBOOT);
    delay_ms(500);
    CheckBleRecvMsg();
  }
  BLE_UART_SEND(UART1, BLE_CMD_MODE_DATA);
  delay_ms(200);
  DBG_PRINT("rename OK\n");
}

#if USE_GSENSOR_JUMP_DETECT
static void RL_InitGsensorCalibState(uint32_t now)
{
  g_gs_calib_started = 0u;
  g_gs_calib_done = 0u;
  g_gs_calib_schedule_at = now + GS_CAL_START_DELAY_MS;
  g_gs_calib_stable_since = 0u;
  g_gs_calib_last_sample_time = 0u;
  g_gs_calib_window_idx = 0u;
  g_gs_calib_window_count = 0u;
  g_gs_calib_stable_now = 0u;
  for (uint32_t i = 0; i < GS_CAL_STABLE_WINDOW_SAMPLES; i++)
  {
    g_gs_calib_window[i] = 0.0f;
  }
  SetGreenLedMode(GS_CAL_LED_UNCAL_FREQ_HZ, GS_CAL_LED_DUTY);
}

static uint8_t RL_UpdateGsensorStable(uint32_t now)
{
  if (!is_timeout(g_gs_calib_last_sample_time, GS_CAL_STABLE_SAMPLE_INTERVAL_MS))
  {
    return g_gs_calib_stable_now;
  }

  g_gs_calib_last_sample_time = now;

  int16_t axis[3];
  GsensorReadAxis(axis);
  float mag = Gsensor_CalcMagnitude_g_from_raw(axis);

  g_gs_calib_window[g_gs_calib_window_idx] = mag;
  g_gs_calib_window_idx = (g_gs_calib_window_idx + 1u) % GS_CAL_STABLE_WINDOW_SAMPLES;
  if (g_gs_calib_window_count < GS_CAL_STABLE_WINDOW_SAMPLES)
  {
    g_gs_calib_window_count++;
  }

  float sum = 0.0f;
  for (uint32_t i = 0; i < g_gs_calib_window_count; i++)
  {
    sum += g_gs_calib_window[i];
  }
  float mean = (g_gs_calib_window_count > 0u) ? (sum / (float)g_gs_calib_window_count) : 0.0f;

  float var = 0.0f;
  for (uint32_t i = 0; i < g_gs_calib_window_count; i++)
  {
    float d = g_gs_calib_window[i] - mean;
    var += d * d;
  }
  float stddev = 0.0f;
  if (g_gs_calib_window_count > 0u)
  {
    stddev = sqrtf(var / (float)g_gs_calib_window_count);
  }

  g_gs_calib_stable_now = (stddev <= GS_CAL_STABLE_STDDEV_THRESHOLD_G &&
                           fabsf(mean - 1.0f) <= GS_CAL_STABLE_MAG_TOLERANCE_G);
  return g_gs_calib_stable_now;
}

static void RL_TryStartGsensorCalibration(uint32_t now)
{
  if (g_gs_calib_done || g_gs_calib_started)
  {
    return;
  }

  if (now < g_gs_calib_schedule_at)
  {
    return;
  }

  if (RL_UpdateGsensorStable(now))
  {
    if (g_gs_calib_stable_since == 0u)
    {
      g_gs_calib_stable_since = now;
    }
    if (get_elapsed_ms(g_gs_calib_stable_since) >= GS_CAL_STABLE_REQUIRED_MS)
    {
      JumpDetect_StartCalibration();
      g_gs_calib_started = 1u;
      g_gs_calib_stable_since = 0u;
    }
  }
  else
  {
    g_gs_calib_stable_since = 0u;
  }
}

static void RL_UpdateGsensorCalibrationState(void)
{
  if (g_gs_calib_started && !JumpDetect_IsCalibrating())
  {
    g_gs_calib_started = 0u;
    if (JumpDetect_IsReady())
    {
      g_gs_calib_done = 1u;
    }
  }
}
#endif

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

  if (g_gs_calib_done)
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

  g_usb_charge_mode = RL_DetectUsbChargeMode();
  if (!g_usb_charge_mode)
  {
    PowerLock_Init();
  }
  // SYS_LockReg();
}

static void RL_InitBoardInputs(void)
{
  /* Button, HALL (optional) and G-sensor interrupt input pins + NVIC */
  Init_Buttons_Gsensor();
}

static void RL_InitDrivers(void)
{
  /* Time base */
  Timer_Init();

  /* Sensors */
  GsensorInit(100000, FSR_2G);
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

/**
 * @brief Initialize GPIO pins (LED, buttons, G-sensor, Buzzer)
 */
void InitGpio(void)
{
  /* Legacy wrapper: keep symbol for existing code, but route to new init stage. */
  RL_InitBoardInputs();
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
  RL_InitSystemCore();
  RL_InitBoardInputs();
}

void InitPeripheral(void)
{
  RL_InitDrivers();
  RL_InitApplication();
}

int main()
{
  int16_t axis[3];
  uint8_t mac[5], device_name[5];

  InitSystem();

  if (g_usb_charge_mode)
  {
    RL_RunUsbChargeLoop();
  }

  InitPeripheral();

#if USE_GSENSOR_JUMP_DETECT
  RL_InitGsensorCalibState(get_ticks_ms());
#endif

  /* Run board GPIO test once at startup */
  BoardTest_RunAll();

  DBG_PRINT("System Up\n");
  BLEToRunMode();
// for make BLE setting NAME + MacAddr
#if 1
  RL_BleRenameFlow(device_name, mac);
#endif

  while (1)
  {
    static uint32_t last_print_time = 0;
    static uint32_t last_batt_check_time = 0;
    static uint8_t low_batt = 0;
    static uint8_t poweroff_done = 0;
    static uint32_t last_usb_update = 0;
    uint32_t now = get_ticks_ms();

    TestMode_PollEnter();
    TestMode_RunMenuIfActive();

    if (UsbHidMouse_TestIsActive())
    {
      if (get_elapsed_ms(last_usb_update) >= 1u)
      {
        last_usb_update = now;
        UsbHidMouse_TestUpdate();
      }
      continue;
    }

#if USE_GSENSOR_JUMP_DETECT
    RL_TryStartGsensorCalibration(now);
#endif
    RL_HandleJumpDetect(now, axis);
#if USE_GSENSOR_JUMP_DETECT
    RL_UpdateGsensorCalibrationState();
#endif
    RL_HandleGsensorPrint(now, &last_print_time, axis);
    RL_HandleBatteryCheck(now, &last_batt_check_time, &low_batt);
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
    RL_HandleIdlePowerOff(&poweroff_done);
    RL_UpdateLedState(low_batt);
  }
}

/*** (C) COPYRIGHT 2016 Richlink Technology Corp. ***/
