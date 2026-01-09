# 🔌 OpenCamX Wiring Guide

> [!WARNING]
> **This document contains outdated pin mappings!**
> Please refer to the **[Build Guide](BUILD_GUIDE.md)** for accurate, up-to-date wiring information.

Complete hardware connection guide for ESP32-S3 with camera, display, and peripherals.

---

## 📋 Components Required

| Component | Model | Notes |
|-----------|-------|-------|
| MCU | ESP32-S3-DevKitC-1 | Must have PSRAM |
| Camera | OV2640 | ESP32-CAM compatible |
| Display | ST7735 TFT | 160x128 or 128x128 |
| SD Card | MicroSD module | SPI mode |
| Joystick | Analog XY | 10K potentiometers |
| Buttons | Tactile switches | 6x (A, B, X, Y, Select, Back) |
| LED | White 5mm | For flashlight |
| IR LED | 850nm IR | For night vision (optional) |

---

## 🎛️ Pin Connections

### Camera Module (OV2640)

| Camera Pin | ESP32-S3 GPIO | Notes |
|------------|---------------|-------|
| XCLK | GPIO 15 | External clock |
| SIOD (SDA) | GPIO 4 | I2C data |
| SIOC (SCL) | GPIO 5 | I2C clock |
| D7 | GPIO 16 | Data bit 7 |
| D6 | GPIO 17 | Data bit 6 |
| D5 | GPIO 18 | Data bit 5 |
| D4 | GPIO 12 | Data bit 4 |
| D3 | GPIO 10 | Data bit 3 |
| D2 | GPIO 8 | Data bit 2 |
| D1 | GPIO 9 | Data bit 1 |
| D0 | GPIO 11 | Data bit 0 |
| VSYNC | GPIO 6 | Vertical sync |
| HREF | GPIO 7 | Horizontal ref |
| PCLK | GPIO 13 | Pixel clock |
| PWDN | Not connected | - |
| RESET | Not connected | - |
| 3.3V | 3.3V | Power |
| GND | GND | Ground |

### ST7735 TFT Display

| Display Pin | ESP32-S3 GPIO | Notes |
|-------------|---------------|-------|
| CS | GPIO 5 | Chip select |
| DC | GPIO 16 | Data/command |
| RST | GPIO 17 | Reset |
| SCK | GPIO 18 | SPI clock |
| MOSI | GPIO 23 | SPI data |
| VCC | 3.3V | Power |
| GND | GND | Ground |
| LED | 3.3V | Backlight (or PWM) |

### SD Card Module (SPI Mode)

| SD Pin | ESP32-S3 GPIO | Notes |
|--------|---------------|-------|
| CS | GPIO 15 | Chip select |
| MOSI | GPIO 13 | SPI data out |
| MISO | GPIO 12 | SPI data in |
| CLK | GPIO 14 | SPI clock |
| VCC | 3.3V | Power |
| GND | GND | Ground |

### Joystick

| Joystick Pin | ESP32-S3 GPIO | Notes |
|--------------|---------------|-------|
| VRx | GPIO 34 | X-axis (ADC) |
| VRy | GPIO 35 | Y-axis (ADC) |
| SW | GPIO 19 | Select button |
| VCC | 3.3V | Power |
| GND | GND | Ground |

### Buttons

| Button | ESP32-S3 GPIO | Notes |
|--------|---------------|-------|
| A | GPIO 32 | Primary action |
| B | GPIO 33 | Secondary action |
| X | GPIO 26 | Increase/toggle |
| Y | GPIO 27 | Decrease/toggle |
| Back | GPIO 25 | Exit to menu |

All buttons should be connected with internal pull-up (INPUT_PULLUP).

### LEDs

| LED | ESP32-S3 GPIO | Notes |
|-----|---------------|-------|
| Flash LED | GPIO 4 | White LED, direct GPIO |
| IR LED | GPIO 2 | 850nm IR, PWM control |

Use a 220Ω resistor for the white LED. IR LED may need external transistor driver.

---

## 🔗 Wiring Diagram

```
                    ┌─────────────────────┐
                    │    ESP32-S3-DevKitC │
                    │                     │
    ┌───────────┐   │  GPIO 15 ─── XCLK   │   ┌───────────┐
    │  OV2640   │───│  GPIO 4  ─── SDA    │   │  ST7735   │
    │  CAMERA   │   │  GPIO 5  ─── SCL    │───│  DISPLAY  │
    │           │   │  GPIO 16 ─── D7/DC  │   │           │
    └───────────┘   │  GPIO 17 ─── D6/RST │   └───────────┘
                    │  GPIO 18 ─── D5/SCK │
    ┌───────────┐   │  GPIO 23 ─── MOSI   │   ┌───────────┐
    │ JOYSTICK  │───│  GPIO 34 ─── VRx    │   │  SD CARD  │
    │           │   │  GPIO 35 ─── VRy    │───│  MODULE   │
    └───────────┘   │  GPIO 19 ─── SW     │   └───────────┘
                    │                     │
    ┌───────────┐   │  GPIO 32 ─── BTN_A  │   ┌───────────┐
    │  BUTTONS  │───│  GPIO 33 ─── BTN_B  │   │   LEDs    │
    │  (6x)     │   │  GPIO 26 ─── BTN_X  │───│ Flash/IR  │
    └───────────┘   │  GPIO 27 ─── BTN_Y  │   └───────────┘
                    │  GPIO 25 ─── BACK   │
                    │  GPIO 4  ─── LED    │
                    │  GPIO 2  ─── IR_LED │
                    │                     │
                    │  3.3V ───┬─── VCC   │
                    │  GND  ───┴─── GND   │
                    └─────────────────────┘
```

---

## ⚡ Power Requirements

| Component | Current Draw |
|-----------|-------------|
| ESP32-S3 | ~200mA |
| OV2640 | ~50mA |
| ST7735 | ~25mA |
| SD Card | ~80mA peak |
| White LED | ~20mA |
| IR LED | ~50mA |
| **Total** | **~425mA** |

Use a 5V 1A power supply minimum. USB-C power delivery recommended.

---

## 🔧 Assembly Tips

1. **Camera connection**: Use short, shielded cables for camera data lines
2. **Display orientation**: Check INITR_GREENTAB vs INITR_BLACKTAB for your display
3. **SD Card**: Format as FAT32 before use
4. **Debouncing**: Software debouncing is handled in Input.hpp
5. **PSRAM**: Ensure your ESP32-S3 module has PSRAM enabled

---

## 🧪 Testing Connections

After wiring, upload the firmware and check Serial Monitor:

```
=== OpenCamX OS v1.0 ===
[CAM] Initialized successfully
[LED] Initialized on GPIO 4
[IR] Initialized on GPIO 2
[SD] Mounted: SDHC, Total: 16000MB, Used: 52MB
[INIT] Registered 8 modes
```

If any component fails, check wiring and power connections.
