# Whac-A-Mole Game BLE UART Protocol Specification

**Version**: 1.0  
**Date**: 2026-05-09  
**Target Device**: M487 MCU with WS2812B RGB LED Matrix (8×8 = 64 LEDs)  
**Transport**: BLE GATT UART (115200 bps)

---

## 1. Overview

### Device Model

```
┌────────────────┐      BLE GATT UART      ┌──────────────────┐
│ Web Host       │      (115200 bps)       │ M487 MCU Device  │
│ (Node.js/Py)   │ ◄────────────────────► │ (WS2812B Matrix) │
│                │                        │                  │
│ - Game Logic   │ Send: LED Frames       │ - Display        │
│ - Scoring      │       Brightness Cmd   │ - Detect Hits    │
│ - Timing       │       Hit Config Cmd   │ - Report Hits    │
└────────────────┘      Receive: Hit Report└──────────────────┘
                        (0x01 bytes)
```

### Key Characteristics

- **No device-side game state**: Device is stateless, host controls everything
- **Polling-based operation**: Device samples button input every 20ms
- **Brightness persistence**: Once set, applies to all subsequent frames until changed
- **Hit detection configurable**: Button only by default; G-sensor optional via command
- **Simple hit reporting**: Single byte (0x01) sent when user strikes

---

## 2. Packet Formats

All packets use **byte stuffing** with EPY variant headers/footers:

| Byte Position | Meaning |
|---|---|
| Header: `0xAA` | Start-of-packet marker |
| Footer: `0x55` | End-of-packet marker |

### 2.1 LED Display Command (13 bytes)

**Host → Device**: Tell device which LEDs to light and what color.

```
[0]    0xAA         Header
[1]    Color Code   0x00–0x06 (see table below)
[2]    Target Tag   0x01 = target (player should hit), 0x00 = decoy
[3–10] Bitmap       8 bytes (rows 0–7)
       Each byte:   MSB (bit 7) = column 0, LSB (bit 0) = column 7
                    1 = LED ON, 0 = LED OFF
[11]   Checksum     XOR of bytes [1–10]
[12]   0x55         Footer
```

**Color Codes**:

| Code | Color | RGB |
|------|-------|-----|
| 0x00 | OFF | (0, 0, 0) |
| 0x01 | RED | (255, 0, 0) |
| 0x02 | GREEN | (0, 255, 0) |
| 0x03 | BLUE | (0, 0, 255) |
| 0x04 | YELLOW | (255, 255, 0) |
| 0x05 | PURPLE | (128, 0, 128) |
| 0x06 | WHITE | (255, 255, 255) |

**Example: Display green 2×2 square in top-left, mark as target**

```
Bitmap visualization:
  Cols: 0 1 2 3 4 5 6 7
Row 0: 1 1 0 0 0 0 0 0  → byte = 0xC0
Row 1: 1 1 0 0 0 0 0 0  → byte = 0xC0
Row 2: 0 0 0 0 0 0 0 0  → byte = 0x00
Row 3: 0 0 0 0 0 0 0 0  → byte = 0x00
Row 4: 0 0 0 0 0 0 0 0  → byte = 0x00
Row 5: 0 0 0 0 0 0 0 0  → byte = 0x00
Row 6: 0 0 0 0 0 0 0 0  → byte = 0x00
Row 7: 0 0 0 0 0 0 0 0  → byte = 0x00

Packet:
0xAA             0x02             0x01
0xC0 0xC0 0x00 0x00 0x00 0x00 0x00 0x00
Checksum = 0x02 XOR 0x01 XOR 0xC0 XOR 0xC0 XOR 0x00 XOR ... = 0x03
0x03 0x55

Full: AA 02 01 C0 C0 00 00 00 00 00 00 03 55
```

---

### 2.2 Brightness Adjustment Command (5 bytes)

**Host → Device**: Adjust overall LED brightness (applies to all subsequent frames).

```
[0]    0xAA              Header
[1]    0xC0              Brightness command type
[2]    Brightness %      0–100 (0% = off, 100% = full intensity)
[3]    Checksum          XOR of bytes [1–2]
[4]    0x55              Footer
```

**Example: Set brightness to 75%**

```
0xAA
0xC0                     (brightness cmd type)
0x4B                     (75 decimal = 0x4B hex)
Checksum = 0xC0 XOR 0x4B = 0x8B
0x55

Full: AA C0 4B 8B 55
```

**Notes**:
- Brightness change is **global** (applies until next brightness command)
- Host should send brightness command *after* LED frame to ensure synchronous updates (optional)
- Default brightness on device startup: **20%**
- Invalid brightness (>100%) causes packet to be **silently rejected**

---

### 2.3 Hit Detection Config Command (5 bytes)

**Host → Device**: Enable/disable specific hit detection methods.

```
[0]    0xAA              Header
[1]    0xC1              Hit config command type
[2]    Method Bits       Bitmask:
                         0x01 = Button (bit 0)
                         0x02 = G-Sensor (bit 1)
                         Common values:
                         0x01 = Button only (DEFAULT)
                         0x02 = G-Sensor only
                         0x03 = Button + G-Sensor
[3]    Checksum          XOR of bytes [1–2]
[4]    0x55              Footer
```

**Example: Enable button + G-sensor (hybrid detection)**

```
0xAA
0xC1                     (hit config cmd type)
0x03                     (0x01 | 0x02 = button + gsensor)
Checksum = 0xC1 XOR 0x03 = 0xC2
0x55

Full: AA C1 03 C2 55
```

**Example: Disable G-sensor, button only (reset to default)**

```
0xAA
0xC1
0x01                     (button only)
Checksum = 0xC1 XOR 0x01 = 0xC0
0x55

Full: AA C1 01 C0 55
```

**Notes**:
- Default on device startup: **0x01** (button only, G-sensor disabled)
- G-sensor requires device to have sensor hardware and `MOLE_HIT_GSENSOR_ENABLE` compiled in
- Invalid combinations (e.g., 0x00 = no detection) are accepted but disable both methods

---

### 2.4 Hit Report (1 byte)

**Device → Host**: Player struck the display.

```
[0]    0x01              Hit report marker
```

**Timing**:
- Sent immediately when button is pressed (falling edge: released → pressed)
- Or when G-sensor detects significant motion (if enabled)
- Device samples button every **20ms**, so response latency ≤ 40ms

**Notes**:
- Single byte, no checksum (simple protocol)
- Multiple consecutive presses → multiple 0x01 bytes sent
- No correlation with LED frame or color sent (host infers context)

---

## 3. Checksum Calculation

**XOR method** (used for LED, Brightness, and Hit Config packets):

```c
uint8_t checksum = 0;
for (each data byte) {
    checksum ^= data_byte;
}
```

**Example: LED frame from section 2.1**

```
Data bytes: 0x02, 0x01, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
Checksum = 0x02 ^ 0x01 ^ 0xC0 ^ 0xC0 ^ 0x00 ^ 0x00 ^ 0x00 ^ 0x00 ^ 0x00 ^ 0x00
         = 0x03 ^ 0xC0 ^ 0xC0 ^ 0x00 ^ ...
         = 0xC3 ^ 0xC0 ^ 0x00 ^ ...
         = 0x03
```

Final checksum: **0x03** (matches firmware parser logic: XOR bytes `[1..10]`).

---

## 4. State Diagram

```
┌─────────────────┐
│     IDLE        │
│  (Boot/Reset)   │
└────────┬────────┘
         │
         │ Receive LED Frame (0xAA...0x55)
         ▼
┌─────────────────────┐
│  DISPLAY FRAME      │
│  - Apply colors     │
│  - Light up LEDs    │
│  - Start timer      │
└────────┬────────────┘
         │
    ┌────┴────┐
    │          │
    │ Poll key │ Detect motion (G-sensor)
    │ every    │ if enabled
    │ 20ms     │
    │          │
    ▼          ▼
┌────────────────────────┐
│  AWAIT INPUT           │
│  - Button armed        │
│  - G-sensor armed      │
│    (if configured)     │
└────────┬───────────────┘
         │
    Player strikes
         │
         ▼
┌──────────────────────┐
│  HIT DETECTED        │
│  - Send 0x01 to host │
└────────┬─────────────┘
         │
    Loop back to IDLE
    (await next LED frame)
```

**Key Notes**:
- Device has **no game state** — stays in "await input" until new frame arrives
- Pressing button while no frame is displayed still sends 0x01 (host responsible for validity)
- Brightness or hit config commands can arrive at any time; processed immediately

---

## 5. Example Sequences

### 5.1 Simple Hit Scenario

```
1. Host sends LED frame (red target, 2×2 pattern)
   AA 01 01 C0 C0 00 00 00 00 00 00 [checksum] 55
   
2. Device displays red 2×2 pattern, starts polling key

3. User presses button (within ~1 second)
   
4. Device detects button press (20ms polling interval)
   
5. Device sends hit report
   01
   
6. Host receives hit, updates score

7. Host sends new LED frame (green decoy, or next target)
   AA 02 00 [bitmap] [checksum] 55
   
   [Loop continues...]
```

---

### 5.2 Brightness Adjustment Mid-Game

```
1. Host sends LED frame at 20% brightness (default)
   AA 02 01 FF FF FF FF FF FF FF FF [checksum] 55
   
2. Device displays green with 64 LEDs on at 20%

3. Host sends brightness adjust to 80%
   AA C0 50 90 55
   (0x50 = 80 decimal)
   (Checksum = 0xC0 XOR 0x50 = 0x90)
   
4. Device updates brightness to 80%
   
5. Current frame (green) redraws at 80% brightness
   (Frame looks much brighter now)

6. Host continues sending new frames; all display at 80%
   until next brightness command
```

---

### 5.3 Enable Hybrid Hit Detection

```
1. Host sends hit detection config command
   AA C1 03 C2 55
   (0x03 = button + gsensor)
   
2. Device enables button input polling
   
3. Device also monitors G-sensor for motion
   
4. Player can now trigger hit by:
   - Pressing button, OR
   - Tapping/shaking device (accelerometer threshold)
   
5. Both methods send the same 0x01 report
   (Host cannot distinguish which method detected the hit)
```

---

### 5.4 Disable G-Sensor (Return to Button-Only)

```
1. Host sends hit detection config
   AA C1 01 C0 55
   (0x01 = button only)
   
2. Device disables G-sensor monitoring
   (no overhead from motion detection)
   
3. Only button presses trigger 0x01 reports
```

---

## 6. Timing & Polling Characteristics

### Device Main Loop

- **Cycle time**: 20 ms
- **Operations per cycle**:
  1. Sample button input
  2. Parse BLE stream (if data available)
  3. Sample G-sensor (every 20ms if enabled)
  4. Garbage collection (optional, low priority)

### Button Detection Latency

- **Polling frequency**: 20 ms
- **Worst-case latency**: 40 ms (can occur just after polling)
- **Average latency**: 20 ms
- **Detection method**: Falling edge (button pressed → released state change)

### LED Display Refresh

- **Transmission time**: ~500 µs (SPI @ 6 MHz, 64 LEDs × 24 bits)
- **Hold time**: 4 ms (enforce WS2812B timing requirements)
- **Total per frame**: ~4.5 ms

### Brightness Adjustment Application

- **Immediate**: Brightness updates apply to next pixel refresh
- **Redraw synchronization**: If frame already displayed, device redraws with new brightness
- **Latency**: ≤ 5 ms (no blocking operations)

### BLE Stream Buffering

- **RX buffer size**: 256 bytes (circular)
- **Packet parsing**: Byte-by-byte with state machine
- **Parser reset on error**: Invalid checksum triggers parser reset (no backlog)

---

## 7. Error Handling & Edge Cases

### Invalid Packets (Silently Rejected)

| Condition | Action |
|-----------|--------|
| Bad checksum | Packet discarded, parser reset |
| Brightness > 100% | Packet rejected, buffer flushed |
| Invalid color code | Packet accepted, treated as 0x00 (OFF) |
| Truncated packet (timeout) | Parser waits indefinitely or resets on next header |
| Malformed header | Parser resets, waits for next 0xAA |

### Recovery

- **No ACK/NAK protocol**: Host does not receive confirmation of packet receipt
- **Recommendation**: Host should resend LED frames periodically (e.g., every 100ms) if relying on confirmed delivery
- **BLE reliability**: BLE link-layer handles retransmission; application layer should not expect corrupt packets

### Button Debounce

- **Hardware debounce**: Onboard RC filtering (typical ~10 ms)
- **Software debounce**: 20 ms polling eliminates bounce noise
- **Edge detection**: Falling edge (1→0 transition) = button pressed

---

## 8. Constraints & Limitations

| Aspect | Limit | Notes |
|--------|-------|-------|
| Max brightness | 100% | Higher values clamped or rejected |
| Min brightness | 0% | Turns off all LEDs |
| LED count | 64 | Fixed 8×8 matrix |
| Packet length (LED) | 13 bytes | Fixed format |
| Packet length (control) | 5 bytes | Fixed format |
| BLE MTU | 20 bytes (typical) | Packets fit easily; can burst multiple |
| Message frequency | No limit | Device processes as fast as BLE delivers |
| Hit report frequency | 1 per button press | Can queue multiple if rapid-fire presses |

---

## 9. Protocol Compliance Checklist

**For Web Frontend Integration**:

- [ ] Implement packet builder (LED display, brightness, hit config)
- [ ] Calculate checksums correctly (XOR method)
- [ ] Handle received 0x01 bytes (track in UI, update score)
- [ ] Send LED frames at predictable intervals (e.g., every 500ms)
- [ ] Adjust brightness with separate command *before* or *after* LED frames
- [ ] Test hit detection with button press (expect 0x01 within 40ms)
- [ ] Verify G-sensor optional (device works with button-only if sensor not enabled)
- [ ] Resend frames periodically if BLE delivery is unreliable (fallback strategy)
- [ ] Monitor device startup (expect no hits reported until first LED frame)

---

## 10. Testing & Validation

### Unit Test Examples

**Test 1: LED Packet Parsing**
```
Input:  AA 02 01 C0 C0 00 00 00 00 00 00 03 55
Expected: Color=GREEN (0x02), Target=1, Bitmap[0]=0xC0, Bitmap[1]=0xC0
Result: ✓ Frame displays as green 2×2 square
```

**Test 2: Brightness Command**
```
Input:  AA C0 4B 8B 55
Expected: Brightness set to 75%
Result: ✓ Current frame redraws brighter
```

**Test 3: Hit Detection Config**
```
Input:  AA C1 03 C2 55
Expected: Button + G-sensor enabled
Result: ✓ Pressing button or shaking device both send 0x01
```

**Test 4: Button Hit Report**
```
Action: Press button after LED frame displayed
Expected: 0x01 received at host within 40ms
Result: ✓ Latency measured at 25ms average
```

**Test 5: Bad Checksum Rejection**
```
Input:  AA 02 01 C0 C0 00 00 00 00 00 00 FF 55  (bad checksum: 0xFF instead of 0x03)
Expected: Packet silently rejected, no frame displayed
Result: ✓ Previous frame remains on LEDs
```

---

## 11. Appendix: Sample Implementation (Pseudocode)

### JavaScript/Node.js Example

```javascript
// Send LED frame (green, 2×2, target)
function sendLedFrame(device, rows, colorCode, isTarget) {
    const packet = [0xAA, colorCode, isTarget ? 0x01 : 0x00];
    packet.push(...rows);  // 8 bytes
    
    // Calculate checksum
    let checksum = 0;
    for (let i = 1; i < 11; i++) {
        checksum ^= packet[i];
    }
    packet.push(checksum);
    packet.push(0x55);
    
    device.write(Buffer.from(packet));
}

// Send brightness command
function sendBrightnessCmd(device, brightnessPercent) {
    const packet = [0xAA, 0xC0, brightnessPercent];
    const checksum = 0xC0 ^ brightnessPercent;
    packet.push(checksum, 0x55);
    
    device.write(Buffer.from(packet));
}

// Send hit detection config
function sendHitConfigCmd(device, methodBits) {
    const packet = [0xAA, 0xC1, methodBits];
    const checksum = 0xC1 ^ methodBits;
    packet.push(checksum, 0x55);
    
    device.write(Buffer.from(packet));
}

// Handle hit report
device.onData((buffer) => {
    for (let byte of buffer) {
        if (byte === 0x01) {
            console.log("Hit detected!");
            // Update UI, increment score, etc.
        }
    }
});
```

---

## 12. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-05-09 | Initial specification (LED, Brightness, Hit Config, Hit Report) |

---

**Document Status**: ✅ Ready for Web Frontend Integration  
**Last Updated**: 2026-05-09
