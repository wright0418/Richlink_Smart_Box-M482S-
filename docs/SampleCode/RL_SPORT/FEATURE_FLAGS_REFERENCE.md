# RL_SPORT Mole Feature Flags Reference

This document lists compile-time feature flags that affect firmware behavior, memory usage, and protocol compatibility.

## LED Matrix / Protocol Features

| Macro | Default | Purpose |
| --- | ---: | --- |
| `MOLE_LED_LEGACY8_SUPPORT` | `1` | Keep the legacy 8×8 mono bitmap protocol enabled. This must stay enabled for backward compatibility. |
| `MOLE_ENABLE_RGB16X16` | `1` | Enable 16×16 RGB LED matrix protocol and driver support. Disable to keep the firmware in 8×8-only mode. |
| `MOLE_ENABLE_RGB16X16_COLOR` | `1` | Enable chunked per-pixel RGB transfer for 16×16. Disable to keep only 16×16 mono bitmap support. |
| `MOLE_RGB16_ROWS` | `16` | 16×16 matrix row count. |
| `MOLE_RGB16_COLS` | `16` | 16×16 matrix column count. |
| `MOLE_RGB16_LED_COUNT` | `256` | Total 16×16 pixel count. |
| `MOLE_RGB16_COLOR_BYTES` | `768` | Full RGB frame size: 256 LEDs × 3 bytes. |
| `MOLE_RGB16_CHUNK_PAYLOAD_MAX` | `10` | Max RGB payload bytes in each chunk packet. Kept conservative for BLE UART compatibility. |
| `MOLE_WS2812_LED_COUNT` | `256` when 16×16 enabled, otherwise `64` | Physical WS2812B pixel buffer size used by the driver. |

## Version / Capability Bits

`FW_CAPABILITY_MASK` is compiled from the enabled features and is returned by `AT+TEST,VERSION` and `AT+TEST,CAPABILITIES`.

### Firmware version first digit rule

The first digit of `FW_VERSION` is selected automatically in `SampleCode/RL_SPORT/project_config.h` based on the enabled LED feature flags:

- `1.x.y`: legacy 8×8 mono only (`MOLE_ENABLE_RGB16X16=0`)
- `2.x.y`: 16×16 mono enabled (`MOLE_ENABLE_RGB16X16=1` and `MOLE_ENABLE_RGB16X16_COLOR=0`)
- `3.x.y`: 16×16 RGB color chunk enabled (`MOLE_ENABLE_RGB16X16=1` and `MOLE_ENABLE_RGB16X16_COLOR=1`)

`FW_VERSION` is assembled as `FW_VERSION_MAJOR.FW_VERSION_MINOR.FW_VERSION_PATCH`.

| Bit Macro | Hex | Meaning |
| --- | ---: | --- |
| `FW_CAP_LEGACY_8X8_MONO` | `0x00000001` | Legacy 8×8 mono packet support. |
| `FW_CAP_RGB16_MONO` | `0x00000002` | 16×16 mono bitmap support. |
| `FW_CAP_RGB16_COLOR_CHUNKED` | `0x00000004` | 16×16 per-pixel RGB chunk support. |
| `FW_CAP_BLE_AT_REPL` | `0x00000008` | BLE AT REPL diagnostic commands are compiled in. |
| `FW_CAP_HIT_BUTTON` | `0x00000010` | Button hit detection is compiled in. |
| `FW_CAP_HIT_GSENSOR` | `0x00000020` | G-sensor hit detection is compiled in. |
| `FW_CAP_MOLE_PROFILE` | `0x00000040` | Mole profile firmware branch. |

## Diagnostic Commands

### Version

```text
AT+TEST,VERSION
+OK,VERSION,<FW_VERSION>,<DATE>,<TIME>,CAP=0x0000007F
```

### Capabilities

```text
AT+TEST,CAPABILITIES
+OK,CAPABILITIES,CAP=0x0000007F,LEGACY8=1,RGB16=1,RGB16_COLOR=1,ROWS=16,COLS=16,CHUNK=10
```

## Code Size / RAM Notes

- `MOLE_ENABLE_RGB16X16=0` keeps the driver at the legacy 64-pixel WS2812B buffer size.
- `MOLE_ENABLE_RGB16X16=1` expands the WS2812B pixel/SPI buffers for 256 pixels.
- `MOLE_ENABLE_RGB16X16_COLOR=1` adds a 768-byte RGB staging buffer in `mole_game.c` so partial chunk transfers never display half frames.
- If firmware size or RAM becomes tight, disable `MOLE_ENABLE_RGB16X16_COLOR` first; 16×16 mono requires much less runtime storage.
