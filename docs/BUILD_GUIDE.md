# 🔧 OpenCamX Hardware Build Guide

> **Complete step-by-step guide for assembling your ESP32-S3 based camera platform**

This guide covers everything you need to build your own OpenCamX device, from component selection to final assembly.

---

## 📋 Bill of Materials (BOM)

| Component | Model/Spec | Quantity | Purpose |
|-----------|------------|----------|---------|
| **ESP32-S3-DevKitC-1** | With 8MB PSRAM | 1 | Main controller |
| **OV2640 Camera** | ESP-CAM compatible | 1 | Image capture |
| **ST7735 TFT Display** | 160x128 1.8" SPI | 1 | Live preview UI |
| **MicroSD Module** | SPI interface | 1 | Video recording |
| **Analog Joystick** | 2-axis with button | 1 | Navigation |
| **Tactile Buttons** | 6mm push type | 5 | Controls (A, B, X, Y, Back) |
| **White LED** | 5mm diffused | 1 | Flashlight |
| **IR LED** (optional) | 850nm 5mm | 1-2 | Night vision |
| **Resistors** | 220Ω | 2 | LED current limiting |
| **Wires & Perfboard** | - | As needed | Connections |

> [!IMPORTANT]
> Your ESP32-S3 module **MUST have PSRAM** (8MB recommended). Without PSRAM, camera frame buffers will fail!

---

## 🎛️ Pin Mapping Reference

All pin assignments are designed to avoid conflicts between the camera DVP bus, SPI bus, and strapping pins.

### Camera Module (OV2640) - DVP Parallel Interface

| Camera Pin | ESP32-S3 GPIO | Wire Color (suggested) |
|------------|---------------|------------------------|
| **XCLK** | GPIO 14 | 🟡 Yellow |
| **PCLK** | GPIO 21 | 🟠 Orange |
| **VSYNC** | GPIO 47 | 🔵 Blue |
| **HREF/HSYNC** | GPIO 48 | 🟣 Purple |
| **SIOD (SDA)** | GPIO 4 | 🟢 Green |
| **SIOC (SCL)** | GPIO 5 | ⚪ White |
| **D7** | GPIO 13 | ⬛ |
| **D6** | GPIO 12 | ⬛ |
| **D5** | GPIO 11 | ⬛ |
| **D4** | GPIO 10 | ⬛ |
| **D3** | GPIO 9 | ⬛ |
| **D2** | GPIO 8 | ⬛ |
| **D1** | GPIO 6 | ⬛ |
| **D0** | GPIO 1 | ⬛ |
| **PWDN** | Not connected | - |
| **RESET** | Not connected | - |
| **3.3V** | 3.3V | 🔴 Red |
| **GND** | GND | ⚫ Black |

> [!TIP]
> Use short cables (< 10cm) for data lines D0-D7 to minimize signal interference.

---

### TFT Display (ST7735) - SPI Bus

| Display Pin | ESP32-S3 GPIO | Notes |
|-------------|---------------|-------|
| **CS** | GPIO 42 | Chip Select |
| **DC** | GPIO 2 | Data/Command |
| **RST** | Not connected | Software reset used |
| **SCK** | GPIO 40 | SPI Clock (shared) |
| **MOSI/SDA** | GPIO 39 | SPI Data (shared) |
| **LED/BL** | 3.3V | Backlight always on¹ |
| **VCC** | 3.3V | Power |
| **GND** | GND | Ground |

¹ *For PWM backlight control, connect BL to a free GPIO and modify Display.hpp*

---

### SD Card Module - SPI Bus (Shared)

| SD Pin | ESP32-S3 GPIO | Notes |
|--------|---------------|-------|
| **CS** | GPIO 46 | Chip Select |
| **MOSI** | GPIO 39 | Shared with TFT |
| **MISO** | GPIO 41 | Data In |
| **SCK/CLK** | GPIO 40 | Shared with TFT |
| **VCC** | 3.3V | Power |
| **GND** | GND | Ground |

> [!NOTE]
> TFT Display and SD Card share the same SPI bus (GPIO 39, 40, 41). Only the CS pins differ. This is by design!

---

### Joystick - Analog Inputs

| Joystick Pin | ESP32-S3 GPIO | Notes |
|--------------|---------------|-------|
| **VRx (X-axis)** | GPIO 19 | ADC2 Channel |
| **VRy (Y-axis)** | GPIO 20 | ADC2 Channel |
| **SW (Push)** | GPIO 3 | Internal pull-up |
| **VCC** | 3.3V | Power |
| **GND** | GND | Ground |

> [!CAUTION]
> Do NOT use GPIO 35-39 for joystick when WiFi is active (ADC2 conflict).

---

### Buttons - Digital Inputs

| Button | ESP32-S3 GPIO | Function |
|--------|---------------|----------|
| **A** | GPIO 15 | Primary action / Select |
| **B** | GPIO 16 | Secondary / LED toggle |
| **X** | GPIO 17 | Increase value |
| **Y** | GPIO 18 | Decrease value |
| **Back** | GPIO 7 | Exit to menu |

All buttons connect between GPIO and GND (active LOW with internal pull-up).

```
    GPIO ──[BUTTON]── GND
```

---

### LEDs - PWM Outputs

| LED | ESP32-S3 GPIO | Notes |
|-----|---------------|-------|
| **Flash (White)** | GPIO 38 | 220Ω resistor required |
| **IR (Night Vision)** | GPIO 45 | 220Ω resistor, optional |

**Wiring diagram for LEDs:**
```
    GPIO 38 ──[220Ω]──[LED]── GND
                      (+) (-)
```

---

## 🔌 Complete Wiring Diagram

```
                       ┌─────────────────────────────┐
                       │     ESP32-S3-DevKitC-1      │
                       │                             │
 ╔═══════════╗        │  Camera (DVP Interface)     │        ╔═══════════╗
 ║  OV2640   ║        │                             │        ║  ST7735   ║
 ║  CAMERA   ║        │  GPIO 14 ────── XCLK        │        ║  DISPLAY  ║
 ║           ║────────│  GPIO 21 ────── PCLK        │────────║           ║
 ║ D0-D7     ║        │  GPIO 47 ────── VSYNC       │        ║ SPI Bus   ║
 ║ XCLK/PCLK ║        │  GPIO 48 ────── HREF        │        ║ GPIO 39,40║
 ║ VSYNC/HREF║        │  GPIO 4  ────── SDA (I2C)   │        ║ CS: 42    ║
 ║ SDA/SCL   ║        │  GPIO 5  ────── SCL (I2C)   │        ║ DC: 2     ║
 ╚═══════════╝        │  GPIO 1,6,8-13 ─ D0-D7      │        ╚═══════════╝
                       │                             │
 ╔═══════════╗        │  SPI Bus (Shared)           │        ╔═══════════╗
 ║ JOYSTICK  ║        │                             │        ║  SD CARD  ║
 ║           ║────────│  GPIO 39 ────── MOSI        │────────║  MODULE   ║
 ║ VRx: 19   ║        │  GPIO 40 ────── SCK         │        ║           ║
 ║ VRy: 20   ║        │  GPIO 41 ────── MISO        │        ║ CS: 46    ║
 ║ SW:  3    ║        │  GPIO 42 ────── TFT_CS      │        ║ SPI Bus   ║
 ╚═══════════╝        │  GPIO 46 ────── SD_CS       │        ╚═══════════╝
                       │                             │
 ╔═══════════╗        │  Inputs (Buttons)           │        ╔═══════════╗
 ║  BUTTONS  ║        │                             │        ║   LEDs    ║
 ║           ║────────│  GPIO 15 ────── BTN_A       │────────║           ║
 ║ All to GND║        │  GPIO 16 ────── BTN_B       │        ║ Flash: 38 ║
 ║           ║        │  GPIO 17 ────── BTN_X       │        ║ IR: 45    ║
 ╚═══════════╝        │  GPIO 18 ────── BTN_Y       │        ╚═══════════╝
                       │  GPIO 7  ────── BTN_BACK   │
                       │                             │
                       │  Power                      │
                       │  3.3V ──┬── All VCC pins   │
                       │  GND  ──┴── All GND pins   │
                       └─────────────────────────────┘
```

---

## 🔋 Power Requirements

| Component | Typical Current | Peak Current |
|-----------|----------------|--------------|
| ESP32-S3 | 80mA | 500mA (WiFi TX) |
| OV2640 | 50mA | 100mA |
| ST7735 TFT | 25mA | 40mA |
| SD Card | 30mA | 100mA (write) |
| White LED | 20mA | 20mA |
| IR LED | 50mA | 100mA |
| **Total** | **~255mA** | **~860mA** |

> [!IMPORTANT]
> Use a **5V 2A power supply** minimum. USB 2.0 ports (500mA) may cause brownouts during video recording!

---

## 🛠️ Step-by-Step Assembly

### Step 1: Prepare the ESP32-S3

1. Place ESP32-S3-DevKitC on breadboard or perfboard
2. Verify PSRAM is present: check for **ESP32-S3-WROOM-1-N16R8** or similar
3. Install USB drivers if needed (CP210x or CH340)

### Step 2: Connect Camera Module

1. Wire I2C first: GPIO 4 → SDA, GPIO 5 → SCL
2. Wire control signals: XCLK, PCLK, VSYNC, HREF
3. Wire parallel data bus: D0-D7 (8 wires)
4. Connect power last: 3.3V and GND

> [!WARNING]
> **Double-check camera 3.3V connection!** Connecting 5V will destroy the OV2640 sensor.

### Step 3: Connect Display & SD Card

1. Wire shared SPI bus: GPIO 39 (MOSI), 40 (SCK), 41 (MISO)
2. Wire TFT CS to GPIO 42, DC to GPIO 2
3. Wire SD CS to GPIO 46
4. Connect 3.3V and GND

### Step 4: Connect Joystick

1. Wire VRx to GPIO 19, VRy to GPIO 20
2. Wire SW (push button) to GPIO 3
3. Connect VCC to 3.3V, GND to GND

### Step 5: Connect Buttons

```
For each button:
1. One terminal → GPIO pin
2. Other terminal → GND
(No external resistors needed - uses internal pull-up)
```

Connect buttons to: GPIO 15 (A), 16 (B), 17 (X), 18 (Y), 7 (Back)

### Step 6: Connect LEDs

```
GPIO 38 ──[220Ω]──[LED+]──[LED-]── GND  (White Flash)
GPIO 45 ──[220Ω]──[LED+]──[LED-]── GND  (IR optional)
```

### Step 7: Format SD Card

1. Insert MicroSD into computer
2. Format as **FAT32** (max 32GB) or **exFAT** (64GB+)
3. Eject safely and insert into SD module

---

## ✅ Pre-Flight Checklist

Before powering on, verify:

- [ ] All 3.3V connections are correct (NOT 5V to sensors)
- [ ] Camera ribbon cable is seated properly
- [ ] SD card is formatted FAT32
- [ ] No shorts between adjacent pins
- [ ] USB cable supports data (not charge-only)
- [ ] All button terminals connect to GND

---

## 🧪 Testing Your Build

### 1. Upload Firmware

```bash
git clone https://github.com/Debyte404/CamForge.git
cd CamForge
pio run -e esp32s3 -t upload
pio device monitor -b 115200
```

### 2. Check Serial Output

Successful boot should show:
```
=== OpenCamX OS v1.0 ===
[CAM] PSRAM available: 8388608 bytes
[CAM] Initialized successfully
[LED] Initialized on GPIO 38
[IR] Initialized on GPIO 45
[SD] Mounted: SDHC, Total: 16000MB, Used: 52MB
[INIT] Registered 8 modes
```

### 3. Troubleshooting

| Issue | Possible Cause | Solution |
|-------|---------------|----------|
| `[CAM] Init failed: 0x103` | Wrong camera pins | Verify D0-D7 wiring |
| `[SD] Mount failed!` | SD not formatted | Format as FAT32 |
| Display blank | Wrong CS/DC pins | Check GPIO 42 and GPIO 2 |
| Joystick not working | ADC2 conflict | Disable WiFi for testing |
| PSRAM not found | Wrong ESP32-S3 module | Need -N16R8 variant |

---

## 📐 Enclosure Recommendations

For a clean handheld build:

- **3D Printed Case**: STL files coming soon
- **Minimum dimensions**: 80mm x 50mm x 30mm
- **Display cutout**: 35mm x 28mm
- **Joystick hole**: 18mm diameter
- **Button holes**: 6mm diameter x 5

---

## 🔗 Related Documentation

- [README](../README.md) - Project overview
- [Architecture](ARCHITECTURE.md) - Software design
- [WIRING](WIRING.md) - Legacy pin reference

---

## 📞 Need Help?

If you encounter issues:

1. Check [GitHub Issues](https://github.com/Debyte404/CamForge/issues)
2. Join the community discussions
3. Open a new issue with your serial output

Happy building! 🎉
