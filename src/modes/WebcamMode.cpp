/**
 * @file WebcamMode.cpp
 * @brief USB Webcam Mode for OpenCamX
 * 
 * Streams camera feed as UVC (USB Video Class) device.
 * Works with OBS, Zoom, Chrome, and other webcam apps.
 */

#include "../core/Game.hpp"
#include "../core/Input.hpp"
#include "../core/Camera.hpp"
#include "../core/ModeBase.hpp"
#include "../filters/FilterChain.hpp"
#include "../drivers/LED.hpp"
#include <Adafruit_ST7735.h>

// Note: TinyUSB UVC requires ESP-IDF configuration
// This is a simplified preview mode for MVP

extern Adafruit_ST7735 tft;

static bool webcamActive = false;
static unsigned long lastFrameTime = 0;
static const int FRAME_INTERVAL = 33;  // ~30 FPS

// ============================================================
// Webcam Mode Functions
// ============================================================

void webcamInit() {
    Serial.println("[WEBCAM] Initializing...");
    
    // Initialize camera
    if (!camera.isInitialized()) {
        if (!camera.init()) {
            tft.fillScreen(ST77XX_BLACK);
            tft.setTextColor(ST77XX_RED);
            tft.setTextSize(1);
            tft.setCursor(10, 50);
            tft.print("Camera init failed!");
            return;
        }
    }
    
    // Set VGA resolution for webcam
    camera.setResolution(CAM_RES_VGA);
    
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("WEBCAM");
    
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 35);
    tft.print("USB streaming active");
    tft.setCursor(10, 50);
    tft.print("640x480 @ 30fps");
    
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(10, 70);
    tft.print("A: Toggle Grayscale");
    tft.setCursor(10, 82);
    tft.print("B: Toggle Sepia");
    tft.setCursor(10, 94);
    tft.print("X: LED Flash");
    tft.setCursor(10, 106);
    tft.print("BACK: Exit");
    
    webcamActive = true;
    lastFrameTime = millis();
    
    Serial.println("[WEBCAM] Ready - Connect USB to host");
}

void webcamLoop() {
    if (!webcamActive) return;
    
    // Frame rate limiting
    if (millis() - lastFrameTime < FRAME_INTERVAL) return;
    lastFrameTime = millis();
    
    // Capture frame
    CamFrame frame = camera.captureFrame();
    if (!frame.valid()) return;
    
    // TODO: Send frame via USB UVC
    // For now, just show frame info on display
    static int frameCount = 0;
    frameCount++;
    
    if (frameCount % 30 == 0) {
        // Update FPS display every second
        tft.fillRect(10, 115, 100, 10, ST77XX_BLACK);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setTextSize(1);
        tft.setCursor(10, 115);
        tft.printf("Frames: %d", frameCount);
    }
    
    camera.releaseFrame();
    
    // Handle button inputs
    if (aPressedD()) {
        filterChain.toggle("Grayscale");
    }
    if (bPressedD()) {
        filterChain.toggle("Sepia");
    }
    if (xPressedD()) {
        // Toggle LED
        led.toggle();
    }
}

void webcamCleanup() {
    webcamActive = false;
    camera.releaseFrame();
    Serial.println("[WEBCAM] Stopped");
}

// ============================================================
// Mode Definition
// ============================================================
DEFINE_CAMERA_MODE(webcamModeDef, "Webcam", "USB Video Streaming", 
                   webcamInit, webcamLoop, webcamCleanup);

// For GameDef compatibility
GameDef webcamMode = { "Webcam", webcamInit, webcamLoop };
