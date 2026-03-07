RL_SPORT V3 - Board Test Guide
==============================

Overview
--------
This board test is a quick hardware check for RL_SPORT V3.

Current test items:

- LED (PB3)
- Buzzer (PC7)
- Power lock (PA11)
- Battery ADC (PB1 / EADC0_CH1)
- G-sensor I2C read
- Key (PB15) is optional interactive test (not in default auto sequence)

Boot behavior
-------------
- Firmware always gives one short boot beep.
- `BoardTest_RunAll()` is optional and controlled by `BOARD_TEST_AUTORUN` in `project_config.h`:
  - `0`: do not auto-run board test at boot (default)
  - `1`: run board test once at boot

Pin mode rule (important)
-------------------------
- PB15 and PA11 use **Quasi mode** in app flow.
- Reason: pin state can be read back while still keeping pull-high behavior.

How to run
----------
1. Build `SampleCode/RL_SPORT`.
2. Connect UART0 (PB12/PB13), 115200-8-N-1.
3. Choose one method:
   - Set `BOARD_TEST_AUTORUN = 1` and reboot, or
   - Call `BoardTest_RunAll()` from a debug path/test entry.

UART log format
---------------
Board test prints simple English logs:

- `[BT] <ITEM>: PASS`
- `[BT] <ITEM>: FAIL - <hint>`
- `[BT] <ITEM>: SKIP`
- Final summary: `[BT] SUMMARY: PASS=x FAIL=y SKIP=z`

Current PASS/FAIL criteria
--------------------------
- `LED`: manual check (must blink 3 times)
- `BUZZER`: manual check (must beep 2 times)
- `POWER_LOCK`: set PA11 high, read back in Quasi mode (must read HIGH)
- `BATTERY_ADC`: battery voltage must be in 2.0V ~ 5.5V
- `GSENSOR_I2C`: multiple samples must not be all zeros
- `KEY`: optional test item (default SKIP in quick run)

Operator test steps (recommended)
---------------------------------
1. Run board test and watch UART logs.
2. Check LED and buzzer physically.
3. If fail appears, follow hint text directly:
   - `POWER_LOCK` fail: check PA11 path and power lock circuit
   - `BATTERY_ADC` fail: check PB1 divider and ADC path
   - `GSENSOR_I2C` fail: check I2C wiring/sensor address/pull-ups
4. Confirm final summary has `FAIL=0`.

Key test notes (PB15)
---------------------
If key test reports pressed without pressing:

1. Run test with no touch for full timeout.
2. Measure PB15 level:
   - idle should stay HIGH
   - press should go LOW
3. If unstable, check key pull-up path and board noise coupling.

Integration example
-------------------
```c
int main(void)
{
    SYS_Init();
    UART_Open(UART0, 115200);
    BoardTest_RunAll();
    while (1) { }
}
```
