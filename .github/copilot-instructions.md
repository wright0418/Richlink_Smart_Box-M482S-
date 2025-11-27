# Copilot instructions for this repo (Nuvoton M480)

These rules help AI coding agents work productively in this CMSIS/Keil-style firmware repo for Nuvoton M480 series. Prefer concrete patterns below over generic advice.

## Architecture and layout
- Monorepo of CMSIS + Nuvoton StdDriver samples.
- Libraries: `Library/Device/Nuvoton/M480` (device headers, startup, system), `Library/StdDriver/{inc,src}` (HAL drivers), `Library/CMSIS/Include`.
- Each sample under `SampleCode/**/<Project>/`; VS Code + CMSIS-Toolbox config in `<Project>/VSCode/*.csolution.yml` and `*.cproject.yml`.
- Example (Smart_Box): entry `SampleCode/Smart_Box/main.c`; startup `.../Source/GCC/startup_M480.S` (GCC) or `.../ARM/startup_M480_vsac6.S` (AC6); system `.../Source/system_M480.c`.

## Build, flash, run (VS Code tasks)
- Use workspace tasks wired to CMSIS-Toolbox + pyOCD (see workspace tasks):
  - CMSIS TargetInfo → list targets via csolution/cbuild
  - CMSIS Erase → `pyocd erase --probe cmsisdap: --chip`
  - CMSIS Load → `pyocd load --probe cmsisdap: --cbuild-run ${command:cmsis-csolution.getCbuildRunFile}`
  - CMSIS Run → `pyocd gdbserver --port 3333 --probe cmsisdap: --connect attach --persist --reset-run`
- Default device for Smart_Box csolution: `Nuvoton::M487JIDAE`; build types: `debug`, `release`; toolchains: AC6 and GCC.
- Typical flow: open `SampleCode/Smart_Box/VSCode/Smart_Box.csolution.yml` → run CMSIS Load → optionally run CMSIS Run and attach GDB (localhost:3333).

## Conventions and patterns
- C standard: GCC uses `-std=gnu11`, AC6 uses microlib with `-std=c99`; both enable function/data section GC and DWARF-4.
- Include paths come from cproject: `Library/Device/Nuvoton/M480/Include`, `Library/StdDriver/inc`, `Library/CMSIS/Include`.
- Init flow pattern (Smart_Box): `SYS_Init()` unlocks regs, enables HXT, sets PLL (192 MHz), configures PCLK, enables module clocks + MFP; `main()` then opens UART and prints via `retarget.c`.
- Add peripherals by following `SampleCode/StdDriver/*` recipes (clock enable → module clock select → MFP → driver init).
- When adding sources/headers, update `<Project>.cproject.yml` under `groups:` and `add-path:`; keep compiler-conditional entries (`for-compiler`).

## Linker and startup
- GCC linker script: `Library/Device/Nuvoton/M480/Source/GCC/gcc_arm.ld` (set in `linker:` section of cproject).
- Only include one startup file per compiler; the `groups: CMSIS` section already gates files by `for-compiler`.

## Board/IO specifics (Smart_Box)
- `SampleCode/Smart_Box/AGNETS.md` documents intended IO (LEDs PB1/2/3, RS485 DIR PB14, etc.) and mentions M482; csolution targets M487. Confirm actual target/pinout before flashing and align MFP settings.

## Board/IO specifics (RL_SPORT)
- LED PB3,Blue 
- I2C0 G-Sensor (PB5,PB4) , 中斷 PC5 , Sensor Part number MXC4005XC
- UART1 (PA8,PA9) for BLE Module buardrate = 115200 , AT CMD
- Buzzer PC7 , external have BJT driver ,disable buzzer must drive to low
- button Key PB15 
- dual HALL sensor input PB8 and PB7 , low active
- Deep power down wakeUp Pin PC0


## External tools
- Requires CMSIS-Toolbox and pyOCD on PATH; CMSIS-DAP probe expected. GDB attaches to 3333 when CMSIS Run is active.

## Safe edits checklist
- Don’t mix AC6 and GCC startup in one build; keep guards.
- Mirror relative paths/case when adding files; update `groups:` and `add-path:`.
- For new peripherals: clock enable → clock source → pin mux (MFP) → driver init.
- If changing device/memory, update csolution device and the GCC linker script.
