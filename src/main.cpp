#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "core/Display.hpp"
#include "core/Game.hpp"
#include "core/Input.hpp"
#include "core/Splash.hpp"
#include "core/Menu.hpp"
#include "core/Safety.hpp"

// Camera system includes (ESP32-S3 only)
#ifdef CONFIG_IDF_TARGET_ESP32S3
#include "core/Camera.hpp"
#include "core/ModeBase.hpp"
#include "core/WiFiConfig.hpp"
#include "core/OTA.hpp"
#include "core/OTAWebUI.hpp"
#include "drivers/LED.hpp"
#include "drivers/IRLed.hpp"
#include "drivers/SDCard.hpp"
#include "filters/FilterChain.hpp"
#endif

extern Adafruit_ST7735 tft;

// actual storage for the externs
GameDef* gameRegistry[MAX_GAMES];
int gameCount = 0;

extern const unsigned char debyte_logo [];

const uint16_t LOGO_WIDTH  = 111;
const uint16_t LOGO_HEIGHT = 111;

void drawBitmapVertical(
  Adafruit_GFX &display,
  int16_t x, int16_t y,
  const uint8_t *bitmap,
  int16_t w, int16_t h,
  uint16_t color)
{
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      // For vertical byte order, bit = (row % 8) inside column byte
      uint8_t byte = pgm_read_byte(bitmap + (i + (j / 8) * w));
      if (byte & (1 << (j & 7))) {
        display.drawPixel(x + i, y + j, color);
      }
    }
  }
}


// === Splash Screen ===
void showSplash() {
  int16_t x = (tft.width()  - LOGO_WIDTH)  / 2;
  int16_t y = (tft.height() - LOGO_HEIGHT) / 2;

  // Fade-in
  for (int b = 0; b <= 255; b += 10) {
    uint16_t color = tft.color565(b, b, b);
    drawBitmapVertical(tft, x, y, debyte_logo, LOGO_WIDTH, LOGO_HEIGHT, color);
    delay(30);
  }

  delay(1000); // Hold logo

  // Fade-out
  for (int b = 255; b >= 0; b -= 10) {
    uint16_t color = tft.color565(b, b, b);
    drawBitmapVertical(tft, x, y, debyte_logo, LOGO_WIDTH, LOGO_HEIGHT, color);
    delay(30);
  }

  tft.fillScreen(ST77XX_BLACK);
}

// === Game Declarations ===
extern GameDef snakeGame;
extern GameDef pongGame;
extern GameDef one;
extern GameDef two;

// === Camera Mode Declarations (ESP32-S3 only) ===
#ifdef CONFIG_IDF_TARGET_ESP32S3
extern GameDef webcamMode;
extern GameDef povMode;
extern GameDef edgeMode;
extern GameDef retroMode;
#endif

// ==============================================
//  Setup / Loop (Corrected Boot Sequence)
// ==============================================

// System state flags
static bool sdCardAvailable = false;
static bool cameraAvailable = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== OpenCamX OS v1.0 ===");
  
  // 1. Initialize safety systems first (watchdog, memory checks)
  safety_init();
  
  // 2. Initialize display and show splash
  tft.initR(INITR_GREENTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  
  // Register games first (always available)
  registerGame(&snakeGame);
  registerGame(&pongGame);
  registerGame(&one);
  registerGame(&two);

  // 3. Initialize SD card (before camera - needed for config/assets)
  #ifdef CONFIG_IDF_TARGET_ESP32S3
  Serial.println("[BOOT] Initializing SD card...");
  if (sdCard.init()) {
    sdCardAvailable = true;
    Serial.println("[BOOT] SD card ready");
  } else {
    sdCardAvailable = false;
    Serial.println("[BOOT] SD card failed - recording disabled");
    // Show SD error icon on display (brief flash)
    tft.fillCircle(tft.width() - 10, 10, 5, ST77XX_RED);
  }
  #endif

  // Register camera modes (ESP32-S3 only)
  #ifdef CONFIG_IDF_TARGET_ESP32S3
  Serial.println("[BOOT] Registering camera modes...");
  registerGame(&webcamMode);
  registerGame(&povMode);
  registerGame(&edgeMode);
  registerGame(&retroMode);
  
  // 4. Initialize camera subsystem
  Serial.println("[BOOT] Initializing camera...");
  if (camera.init()) {
    cameraAvailable = true;
    Serial.println("[BOOT] Camera ready");
  } else {
    cameraAvailable = false;
    Serial.println("[BOOT] Camera init failed - capture disabled");
    // Show "No Camera" icon on display
    tft.setCursor(10, 10);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(1);
    tft.print("NO CAM");
  }
  
  // Initialize LEDs
  led.init();
  irLed.init();
  
  // 5. Initialize WiFi and OTA system
  Serial.println("[BOOT] Initializing WiFi...");
  wifiConfig.init();
  if (wifiConfig.connect()) {
    Serial.println("[BOOT] WiFi connected, starting OTA...");
    // Initialize OTA manager with your GitHub repo
    otaManager.init("Debyte404", "CamForge");
    otaManager.setCheckInterval(7200000);  // Check every 2 hours
    // Start OTA web dashboard
    otaWebUI.init(80);
    Serial.println("[BOOT] OTA Web UI at http://" + wifiConfig.getIPAddress() + "/ota");
  }
  #endif

  // 6. Initialize input and menu
  initInput();
  showSplash();
  initMenu();
  
  Serial.printf("[BOOT] Registered %d modes\n", gameCount);
  Serial.printf("[BOOT] SD: %s, Camera: %s\n", 
                sdCardAvailable ? "OK" : "FAIL",
                cameraAvailable ? "OK" : "FAIL");
}

void loop() {
  // Safety tick - feeds watchdog & checks memory
  safety_tick();
  
  // OTA background tasks (non-blocking)
  #ifdef CONFIG_IDF_TARGET_ESP32S3
  otaManager.loop();   // Periodic update checks
  otaWebUI.loop();     // Handle web requests
  #endif
  
  // Check for critical errors
  if (safety_is_error_state()) {
    Serial.println("[LOOP] Resetting due to errors...");
    safety_clear_errors();
    initMenu();  // Reset to safe state
  }
  
  handleMenuInput();
}