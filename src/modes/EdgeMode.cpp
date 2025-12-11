/**
 * @file EdgeMode.cpp
 * @brief Edge Detection Mode for OpenCamX
 * 
 * Real-time Sobel edge detection on camera feed.
 * Optimized for 320x240 resolution for performance.
 */

#include "../core/Game.hpp"
#include "../core/Input.hpp"
#include "../core/Camera.hpp"
#include "../core/ModeBase.hpp"
#include "../filters/FilterChain.hpp"
#include <Adafruit_ST7735.h>

extern Adafruit_ST7735 tft;

static bool edgeActive = false;
static bool showEdges = true;  // Toggle between raw/edge view
static uint8_t edgeThreshold = 50;

// ============================================================
// Edge Detection Functions
// ============================================================

void drawEdgePreview(CamFrame& frame) {
    // For JPEG frames, we can't directly process pixels
    // This would require decoding - for MVP, show status instead
    
    static int lastUpdate = 0;
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();
    
    tft.fillRect(10, 60, 140, 50, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.printf("Frame: %dx%d", frame.width, frame.height);
    tft.setCursor(10, 75);
    tft.printf("Size: %d bytes", frame.len);
    tft.setCursor(10, 90);
    tft.printf("Edges: %s", showEdges ? "ON" : "RAW");
    tft.setCursor(10, 105);
    tft.printf("Threshold: %d", edgeThreshold);
}

// ============================================================
// Edge Mode Functions
// ============================================================

void edgeInit() {
    Serial.println("[EDGE] Initializing...");
    
    // Initialize camera at lower resolution for performance
    if (!camera.isInitialized()) {
        camera.init();
    }
    camera.setResolution(CAM_RES_QVGA);  // 320x240
    
    // Setup filter chain with edge detection
    filterChain.clear();
    filterChain.addFilter(&edgeDetectFilter);
    edgeDetectFilter.threshold = edgeThreshold;
    edgeDetectFilter.enabled = showEdges;
    
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("EDGE DET");
    
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 35);
    tft.print("A: Toggle Edge/Raw");
    tft.setCursor(10, 47);
    tft.print("X/Y: Threshold +/-");
    
    edgeActive = true;
}

void edgeLoop() {
    if (!edgeActive) return;
    
    // Capture frame
    CamFrame frame = camera.captureFrame();
    if (frame.valid()) {
        drawEdgePreview(frame);
        camera.releaseFrame();
    }
    
    // Handle buttons
    if (aPressedD()) {
        showEdges = !showEdges;
        edgeDetectFilter.enabled = showEdges;
        Serial.printf("[EDGE] %s\n", showEdges ? "Edges ON" : "Raw view");
    }
    
    if (xPressedD() && edgeThreshold < 250) {
        edgeThreshold += 10;
        edgeDetectFilter.threshold = edgeThreshold;
    }
    
    if (yPressedD() && edgeThreshold > 10) {
        edgeThreshold -= 10;
        edgeDetectFilter.threshold = edgeThreshold;
    }
}

void edgeCleanup() {
    edgeActive = false;
    filterChain.clear();
    Serial.println("[EDGE] Mode exited");
}

// ============================================================
// Mode Definition
// ============================================================
DEFINE_CAMERA_MODE(edgeModeDef, "Edge Detect", "Real-time Edge Detection",
                   edgeInit, edgeLoop, edgeCleanup);

// GameDef compatibility
GameDef edgeMode = { "Edge Det", edgeInit, edgeLoop };
