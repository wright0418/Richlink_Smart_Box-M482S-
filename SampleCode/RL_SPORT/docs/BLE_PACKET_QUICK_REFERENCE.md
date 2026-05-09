# BLE GATT UART Packet Quick Reference Guide

**For Web Frontend Developers**  
**Last Updated**: 2026-05-09

---

## TL;DR Packet Formats

### 1️⃣ LED Display Command (13 bytes)
```
┌──────┬────────┬────────┬──────────────────────┬──────────┬────────┐
│ 0xAA │ Color  │ Target │ 8 Bitmap Rows        │ Checksum │ 0x55   │
│ Head │ (0-6)  │ (0/1)  │ (8 bytes, MSB=col0)  │ XOR[1:10]│ Footer │
└──────┴────────┴────────┴──────────────────────┴──────────┴────────┘
  [0]     [1]      [2]         [3:10]              [11]      [12]
  
Colors: 0=OFF, 1=RED, 2=GREEN, 3=BLUE, 4=YELLOW, 5=PURPLE, 6=WHITE
```

**Example**: Display green 2×2 in top-left, mark as target
```javascript
const packet = [
    0xAA,                    // Header
    0x02,                    // Color: GREEN
    0x01,                    // Target: yes
    0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Bitmap (2×2 top-left)
    0x00,                    // Checksum (will calculate)
    0x55                     // Footer
];

// Calculate checksum (XOR bytes 1-10)
let checksum = 0;
for (let i = 1; i <= 10; i++) {
    checksum ^= packet[i];
}
packet[11] = checksum;
```

---

### 2️⃣ Brightness Adjustment Command (5 bytes)
```
┌──────┬────────┬────────────┬──────────┬────────┐
│ 0xAA │ 0xC0   │ Brightness │ Checksum │ 0x55   │
│ Head │ Type   │ 0-100 (%)  │ XOR[1:2]│ Footer │
└──────┴────────┴────────────┴──────────┴────────┘
  [0]    [1]         [2]          [3]       [4]
```

**Example**: Set brightness to 75%
```javascript
const packet = [
    0xAA,           // Header
    0xC0,           // Brightness command type
    75,             // 75%
    0x00,           // Checksum (will calculate)
    0x55            // Footer
];

// Calculate checksum
let checksum = 0xC0 ^ 75;
packet[3] = checksum;
// Result: checksum = 0x8B
```

---

### 3️⃣ Hit Detection Config Command (5 bytes)
```
┌──────┬────────┬────────────┬──────────┬────────┐
│ 0xAA │ 0xC1   │ Method     │ Checksum │ 0x55   │
│ Head │ Type   │ Bits       │ XOR[1:2]│ Footer │
└──────┴────────┴────────────┴──────────┴────────┘
  [0]    [1]       [2]          [3]       [4]
  
Method bits: 0x01=Button, 0x02=G-Sensor
  0x01 = Button only (DEFAULT)
  0x02 = G-Sensor only
  0x03 = Button + G-Sensor (hybrid)
```

**Example**: Enable button + G-sensor
```javascript
const packet = [
    0xAA,           // Header
    0xC1,           // Hit config command type
    0x03,           // Method: 0x01 | 0x02 (button + gsensor)
    0x00,           // Checksum (will calculate)
    0x55            // Footer
];

// Calculate checksum
let checksum = 0xC1 ^ 0x03;
packet[3] = checksum;
// Result: checksum = 0xC2
```

---

### 4️⃣ Hit Report (Device → Host, 1 byte)
```
[0x01]  ← Player hit detected
```

No checksum, just a single byte. Receive this from device when user strikes.

---

## Color Code Reference

| Code | Color | RGB | Usage |
|------|-------|-----|-------|
| 0x00 | OFF | (0, 0, 0) | Hide LED or empty |
| 0x01 | RED | (255, 0, 0) | Target / Hit marker |
| 0x02 | GREEN | (0, 255, 0) | Ready / Active |
| 0x03 | BLUE | (0, 0, 255) | Alternative target |
| 0x04 | YELLOW | (255, 255, 0) | Warning / Caution |
| 0x05 | PURPLE | (128, 0, 128) | Special / Bonus |
| 0x06 | WHITE | (255, 255, 255) | All colors |

---

## Bitmap Row Encoding

Each row is 1 byte: **MSB (bit 7) = column 0**, **LSB (bit 0) = column 7**

```
Bit:    7  6  5  4  3  2  1  0
       [C0 C1 C2 C3 C4 C5 C6 C7]

Example: 2×2 square in top-left
  Row[0] = 0xC0 = 0b11000000 = columns 0,1 ON
  Row[1] = 0xC0 = 0b11000000 = columns 0,1 ON
  Row[2] = 0x00 = 0b00000000 = all OFF
  ... Row[3-7] = 0x00
```

---

## Checksum Calculation

**XOR method** (same as C implementation):

```javascript
function calculateChecksum(bytes, start, end) {
    let checksum = 0;
    for (let i = start; i <= end; i++) {
        checksum ^= bytes[i];
    }
    return checksum;
}

// For LED frame: XOR bytes[1:10]
const ledChecksum = calculateChecksum(packet, 1, 10);

// For control packets: XOR bytes[1:2]
const controlChecksum = calculateChecksum(packet, 1, 2);
```

**Quick examples**:
- `0xC0 ^ 0x50 = 0x90`
- `0xC1 ^ 0x03 = 0xC2`
- `0x02 ^ 0x01 ^ 0xC0 ^ 0xC0 ^ ... = 0xC3` (depends on all bytes)

---

## Helper Functions (JavaScript)

```javascript
/**
 * Build LED display command
 * @param {number} colorCode - 0-6
 * @param {number} targetTag - 0 or 1
 * @param {number[]} bitmapRows - 8 bytes (row0-row7)
 * @returns {Uint8Array} 13-byte packet
 */
function buildLedPacket(colorCode, targetTag, bitmapRows) {
    const packet = new Uint8Array(13);
    packet[0] = 0xAA;
    packet[1] = colorCode;
    packet[2] = targetTag;
    
    for (let i = 0; i < 8; i++) {
        packet[3 + i] = bitmapRows[i];
    }
    
    // Checksum: XOR bytes 1-10
    let checksum = 0;
    for (let i = 1; i <= 10; i++) {
        checksum ^= packet[i];
    }
    packet[11] = checksum;
    packet[12] = 0x55;
    
    return packet;
}

/**
 * Build brightness adjustment command
 * @param {number} brightnessPercent - 0-100
 * @returns {Uint8Array} 5-byte packet
 */
function buildBrightnessPacket(brightnessPercent) {
    const packet = new Uint8Array(5);
    packet[0] = 0xAA;
    packet[1] = 0xC0;
    packet[2] = Math.min(Math.max(brightnessPercent, 0), 100);  // Clamp 0-100
    
    // Checksum: XOR bytes 1-2
    packet[3] = packet[1] ^ packet[2];
    packet[4] = 0x55;
    
    return packet;
}

/**
 * Build hit detection config command
 * @param {number} methodBits - 0x01 (button), 0x02 (gsensor), 0x03 (both)
 * @returns {Uint8Array} 5-byte packet
 */
function buildHitConfigPacket(methodBits) {
    const packet = new Uint8Array(5);
    packet[0] = 0xAA;
    packet[1] = 0xC1;
    packet[2] = methodBits;
    
    // Checksum: XOR bytes 1-2
    packet[3] = packet[1] ^ packet[2];
    packet[4] = 0x55;
    
    return packet;
}

/**
 * Check if received byte is a hit report
 * @param {number} byte
 * @returns {boolean}
 */
function isHitReport(byte) {
    return byte === 0x01;
}
```

---

## Example: Complete Game Round

```javascript
async function playGameRound(bleDevice) {
    // 1. Set default brightness (if not already done)
    const brightnessPacket = buildBrightnessPacket(20);  // 20% default
    await bleDevice.write(brightnessPacket);
    
    // 2. Enable button-only hit detection
    const hitConfigPacket = buildHitConfigPacket(0x01);  // Button only
    await bleDevice.write(hitConfigPacket);
    
    // 3. Create a red target in center
    const centerTarget = [
        0x00, 0x00, 0x00, 0x00,
        0x18, 0x3C, 0x7E, 0x7E,  // Center 4×4 pattern
        0x7E, 0x3C, 0x18, 0x00   // (rows 3-5)
    ];
    const ledPacket = buildLedPacket(0x01, 0x01, centerTarget);  // RED, target=yes
    await bleDevice.write(ledPacket);
    
    // 4. Wait for hit report (with timeout)
    const hitPromise = new Promise((resolve) => {
        const timeout = setTimeout(() => resolve(false), 5000);
        
        bleDevice.onData((buffer) => {
            for (let byte of buffer) {
                if (byte === 0x01) {
                    clearTimeout(timeout);
                    resolve(true);
                }
            }
        });
    });
    
    const playerHit = await hitPromise;
    console.log(playerHit ? "✓ Hit detected!" : "✗ Timeout, no hit");
    
    // 5. Clear display
    const clearPacket = buildLedPacket(0x00, 0x00, [0, 0, 0, 0, 0, 0, 0, 0]);
    await bleDevice.write(clearPacket);
}
```

---

## Timing Reference

| Event | Latency | Notes |
|-------|---------|-------|
| LED display | ~5ms | SPI transmission + 4ms WS2812B hold time |
| Brightness adjust | <5ms | Redraw at new brightness |
| Button hit report | 20–40ms | Polling every 20ms |
| G-sensor hit report | ~20ms | Sampled every 20ms (if enabled) |

**Default device polling cycle**: 20 ms

---

## Error Handling

| Condition | Device Behavior | Frontend Action |
|-----------|-----------------|-----------------|
| Bad checksum | Silently ignored | No response; retry if needed |
| Brightness >100 | Rejected | Clamp to 0-100 before sending |
| Invalid color code | Treated as 0x00 (OFF) | Use 0-6 range only |
| Method bits = 0x00 | Both disabled (safe state) | Use 0x01 or 0x03 normally |

**Recommendation**: Resend LED frames every 500ms if game is active (redundancy strategy).

---

## Device Defaults (on startup)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Brightness | 20% | Changed via 0xC0 packet |
| Hit detection | Button only (0x01) | Changed via 0xC1 packet |
| Display state | Awaiting LED frame | No display until first packet |

---

## Protocol Compliance Checklist

Before integrating:
- [ ] Packet builders correctly format headers/footers (0xAA / 0x55)
- [ ] Checksum calculation using XOR (correct byte ranges)
- [ ] Brightness clamped to 0-100 before sending
- [ ] Color codes limited to 0-6
- [ ] Hit config method bits validated (0x01 / 0x02 / 0x03)
- [ ] Device receives 0x01 bytes within 40ms of button press
- [ ] LED frame displays at specified color + target tag
- [ ] Brightness changes persist across frames

---

## Full Protocol Spec

For complete details (packet definitions, state machine, timing, examples):  
👉 **[docs/BLE_PROTOCOL.md](../docs/BLE_PROTOCOL.md)**

---

**Quick Start**: Copy the helper functions above into your frontend code, use them to build packets, send via BLE, and handle 0x01 bytes for hit detection. ✅
