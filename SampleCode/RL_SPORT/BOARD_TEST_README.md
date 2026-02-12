RL_SPORT V3 - GPIO Board Test
================================

Overview
--------
This is a minimal GPIO hardware test harness for RL_SPORT V3. It exercises:

- LED (PB3)
- Buzzer (PC7)
- Key (PB15) — simple polling (active low)
- HALL inputs (PB7/PB8) — configured as inputs (not actively tested)

How to run
----------
1. Build the `SampleCode/RL_SPORT` project as usual (AC6/toolchain configured in VSCode csolution).
2. Ensure `retarget.c` routes `printf` to `UART0` (PB12/PB13). Connect serial to observe output (115200).
3. Power the board and run the firmware. The test prints progress via UART and blinks LED / beeps buzzer.

Assumptions & Notes
-------------------
- Assumes `SYS_Init()` or equivalent board init already ran before calling `BoardTest_RunAll()`.
- Key (`PB15`) is assumed active-low with external pull (internal pull-up enabled in test).
- I2C / ADC tests are not included in this minimal PoC. If needed, provide schematic info for PB1 and PA11 and whether I2C pull-ups are present.

Integration
-----------
Call `BoardTest_RunAll()` from your `main()` (or add it to a test runner). Example:

    int main(void) {
        SYS_Init();
        UART_Open(UART0, 115200);
        BoardTest_RunAll();
        while (1) { }
    }

Next steps
----------
- Add interrupt-driven key tests, I2C device probe, ADC PB1 measurement, and PA11 power-lock verification once hardware schematic is confirmed.
