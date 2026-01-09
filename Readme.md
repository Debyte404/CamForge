# 📸 OpenCamX OS

> A modular, hackable, open-source camera platform for ESP32-S3

OpenCamX transforms your ESP32-S3 into a **multi-purpose camera system** with USB webcam, video recording, real-time filters, and Python scripting support via the PyForge transpiler.

---

## 🎯 Features

| Feature | Description |
|---------|-------------|
| **USB Webcam** | Stream up to 1600x1200 MJPEG via USB-OTG |
| **POV Recording** | Record video clips to SD card |
| **Edge Detection** | Real-time Sobel edge detection |
| **10+ Filters** | Vintage, Cool, Vibrant, Sepia, Grayscale, Sharpen, Blur |
| **PyForge** | Write mods in Python, compile to C++ |
| **OTA Updates** | Automatic firmware updates from GitHub Releases |
| **OTA Web UI** | Beautiful web dashboard for manual updates |
| **LED Flash** | Built-in flashlight control |
| **IR Night Mode** | PWM-controlled IR LEDs |

> **⚡ CPU Optimized**: All filters use integer math (no floats) for maximum performance on ESP32 without GPU.

---

## 🔧 Hardware Requirements

- **ESP32-S3-DevKitC** (or compatible)
- **OV2640 Camera Module** (ESP32-CAM compatible)
- **ST7735 TFT Display** (160x128)
- **MicroSD Card** (for POV recording)
- **Joystick + Buttons** (for input)
- **Optional:** LED, IR LEDs

### Pin Configuration

| Component | Pins |
|-----------|------|
| TFT Display | CS=5, DC=16, RST=17, SCK=18, MOSI=23 |
| Joystick | X=34, Y=35 |
| Buttons | A=32, B=33, X=26, Y=27, SEL=19, BACK=25 |
| LED Flash | GPIO 4 |
| IR LED | GPIO 2 |
| SD Card | MOSI=13, MISO=12, CLK=14, CS=15 |

---

## 🚀 Quick Start

### 1. Install PlatformIO
```bash
pip install platformio
```

### 2. Clone & Build
```bash
git clone https://github.com/your-repo/OpenCamX.git
cd OpenCamX
pio run -e esp32s3
```

### 3. Flash to Device
```bash
pio run -e esp32s3 -t upload
pio device monitor
```

---

## 📂 Project Structure

```
OpenCamX/
├── src/
│   ├── main.cpp              # Entry point
│   ├── core/
│   │   ├── Camera.hpp        # Camera driver
│   │   ├── Display.hpp       # TFT display
│   │   ├── Game.hpp          # Mode registry
│   │   ├── Input.hpp         # Joystick/buttons
│   │   ├── Menu.hpp          # Menu system
│   │   ├── ModeBase.hpp      # Camera mode interface
│   │   ├── OTA.hpp           # Advanced OTA manager
│   │   └── OTAWebUI.hpp      # OTA web dashboard
│   ├── modes/
│   │   ├── WebcamMode.cpp    # USB webcam
│   │   ├── POVMode.cpp       # Video recording
│   │   ├── EdgeMode.cpp      # Edge detection
│   │   └── RetroMode.cpp     # Vintage filters
│   ├── filters/
│   │   └── FilterChain.hpp   # Image filter pipeline
│   ├── drivers/
│   │   ├── LED.hpp           # Flashlight
│   │   ├── IRLed.hpp         # Night vision
│   │   └── SDCard.hpp        # SD card
│   └── games/
│       ├── Snake.cpp         # Classic games
│       └── Pong.cpp
├── mods/                      # Python mods (compiled by PyForge)
│   └── example_invert.py
├── tools/
│   └── pyforge/              # Python-to-C++ transpiler
│       ├── pyforge.py
│       ├── prebuild.py
│       └── pyforge_runtime.hpp
└── platformio.ini
```

---

## 🐍 PyForge - Write Mods in Python

PyForge lets you write image filters in Python, then compiles them to native C++ at build time.

### Example Filter (mods/my_filter.py)
```python
from camforge import Filter, Frame

class InvertFilter(Filter):
    def process(self, frame: Frame) -> Frame:
        for y in range(frame.height):
            for x in range(frame.width):
                r, g, b = frame.get_pixel(x, y)
                frame.set_pixel(x, y, 255 - r, 255 - g, 255 - b)
        return frame
```

### Manual Compilation
```bash
python tools/pyforge/pyforge.py -i mods/my_filter.py -o src/mods/my_filter.cpp
```

PyForge automatically compiles all `.py` files in `mods/` during PlatformIO build.

---

## 📸 Camera Modes

### Webcam Mode
- Streams via USB Video Class (UVC)
- 640x480 @ 30fps MJPEG
- Filter toggles: A=Grayscale, B=Sepia, X=LED

### POV Mode
- Records to SD card (`/recordings/VID_*.avi`)
- Press A to start/stop recording
- LED flash indicator on save

### Edge Detection
- Real-time Sobel edge detection
- 320x240 for performance
- Adjustable threshold: X/Y buttons

### Retro Mode
- Sepia, grain, vignette filters
- Navigate with joystick
- Adjust intensity: X/Y buttons

---

## 🔄 OTA Updates

CamForge features a **hyper-advanced OTA system** that automatically checks GitHub Releases for new firmware.

### Automatic Updates

Once connected to WiFi, your device will:
1. Check for updates every 2 hours (configurable)
2. Compare semantic versions (vX.Y.Z)
3. Download and install automatically (or notify via callback)

### Web Dashboard

Access the OTA web interface at `http://<device-ip>/ota`:

- View current and latest versions
- Check for updates manually
- Install updates with one click
- Real-time progress display

### Programmatic Control

```cpp
// Initialize (already done in main.cpp)
otaManager.init("Debyte404", "CamForge");
otaManager.setCheckInterval(3600000);  // 1 hour

// Manual check
if (otaManager.checkForUpdate()) {
    Serial.println("Update available!");
    otaManager.performUpdate();  // Downloads and reboots
}

// Get version info
String current = otaManager.getCurrentVersion();  // "v1.0.0"
String latest = otaManager.getLatestVersion();    // "v1.1.0"
```

### Publishing Updates (For Developers)

1. Create a version tag:
   ```bash
   git tag v1.1.0
   git push origin v1.1.0
   ```

2. GitHub Actions automatically:
   - Builds the firmware
   - Creates a Release with the `.bin` attached
   - Devices detect the update within 2 hours

---

## 🎮 Controls

| Button | Action |
|--------|--------|
| **Joystick** | Navigate menu / adjust settings |
| **A** | Select / Toggle primary |
| **B** | Toggle secondary / LED |
| **X/Y** | Increase/Decrease values |
| **BACK** | Exit to menu |

---

## 📜 License

MIT License - Use freely, mod freely, share freely.

---

## 🙏 Credits

- **Teerth Sharma** - owner of seal cult
- **Debyte** - Original game platform
- **Espressif** - ESP32-S3 & esp32-camera
- **Adafruit** - GFX & ST7735 libraries

