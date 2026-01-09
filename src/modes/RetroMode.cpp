/**
 * @file RetroMode.cpp
 * @brief Retro/Film Style Mode for OpenCamX
 * 
 * Combines sepia, grain, and vignette filters
 * for a vintage film camera look.
 */

#include "../core/Game.hpp"
#include "../core/Input.hpp"
#include "../core/Camera.hpp"
#include "../core/ModeBase.hpp"
#include "../filters/FilterChain.hpp"
#include "../drivers/LED.hpp"
#include <Adafruit_ST7735.h>

extern Adafruit_ST7735 tft;

// Define missing color constant (not in Adafruit library)
#ifndef ST77XX_DARKGREY
#define ST77XX_DARKGREY 0x7BEF
#endif

static bool retroActive = false;
static int activeFilter = 0;  // Which filter is selected for adjustment

const char* filterNames[] = {"Sepia", "Grain", "Vignette"};
const int NUM_FILTERS = 3;

// ============================================================
// Retro Mode Functions
// ============================================================

void drawRetroUI() {
    tft.fillRect(0, 50, tft.width(), 70, ST77XX_BLACK);
    
    tft.setTextSize(1);
    tft.setCursor(10, 55);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Active Filters:");
    
    for (int i = 0; i < NUM_FILTERS; i++) {
        ImageFilter* filter = nullptr;
        switch (i) {
            case 0: filter = &sepiaFilter; break;
            case 1: filter = &grainFilter; break;
            case 2: filter = &vignetteFilter; break;
        }
        
        int y = 68 + i * 12;
        
        // Highlight selected filter
        if (i == activeFilter) {
            tft.fillRect(5, y - 2, tft.width() - 10, 12, ST77XX_BLUE);
            tft.setTextColor(ST77XX_WHITE);
        } else {
            tft.setTextColor(filter->enabled ? ST77XX_GREEN : ST77XX_DARKGREY);
        }
        
        tft.setCursor(10, y);
        tft.print(filterNames[i]);
        tft.print(": ");
        tft.print(filter->enabled ? "ON" : "OFF");
        
        // Show intensity for grain/vignette
        if (i == 1 && filter->enabled) {
            tft.printf(" (%d)", grainFilter.intensity);
        } else if (i == 2 && filter->enabled) {
            tft.printf(" (%.1f)", vignetteFilter.getStrength());
        }
    }
    
    // Controls hint
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(10, 108);
    tft.print("UP/DN:Select A:Toggle X/Y:Adj");
}

void retroInit() {
    Serial.println("[RETRO] Initializing...");
    
    // Initialize camera
    if (!camera.isInitialized()) {
        camera.init();
    }
    camera.setResolution(CAM_RES_VGA);
    
    // Initialize LED for flash
    led.init();
    
    // Setup filter chain with retro filters
    filterChain.clear();
    filterChain.addFilter(&sepiaFilter);
    filterChain.addFilter(&grainFilter);
    filterChain.addFilter(&vignetteFilter);
    
    // Default: all enabled with moderate settings
    sepiaFilter.enabled = true;
    grainFilter.enabled = true;
    grainFilter.intensity = 30;
    vignetteFilter.enabled = true;
    vignetteFilter.setStrength(0.5f);  // Use Q8 API for performance
    
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_ORANGE);
    tft.setTextSize(2);
    tft.setCursor(15, 10);
    tft.print("RETRO CAM");
    
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(10, 35);
    tft.print("~ Vintage Film Look ~");
    
    retroActive = true;
    activeFilter = 0;
    
    drawRetroUI();
}

void retroLoop() {
    if (!retroActive) return;
    
    // Handle direction input for filter selection
    Direction dir = readJoystickStateChange();
    if (dir == DIR_UP && activeFilter > 0) {
        activeFilter--;
        drawRetroUI();
    } else if (dir == DIR_DOWN && activeFilter < NUM_FILTERS - 1) {
        activeFilter++;
        drawRetroUI();
    }
    
    // Toggle active filter
    if (aPressedD()) {
        ImageFilter* filter = nullptr;
        switch (activeFilter) {
            case 0: filter = &sepiaFilter; break;
            case 1: filter = &grainFilter; break;
            case 2: filter = &vignetteFilter; break;
        }
        if (filter) {
            filter->enabled = !filter->enabled;
            drawRetroUI();
        }
    }
    
    // Adjust intensity
    if (xPressedD()) {
        switch (activeFilter) {
            case 1:  // Grain
                grainFilter.intensity = min(255, grainFilter.intensity + 10);
                break;
            case 2:  // Vignette
                vignetteFilter.setStrength(min(1.0f, vignetteFilter.getStrength() + 0.1f));
                break;
        }
        drawRetroUI();
    }
    
    if (yPressedD()) {
        switch (activeFilter) {
            case 1:  // Grain
                grainFilter.intensity = max(0, grainFilter.intensity - 10);
                break;
            case 2:  // Vignette
                vignetteFilter.setStrength(max(0.0f, vignetteFilter.getStrength() - 0.1f));
                break;
        }
        drawRetroUI();
    }
    
    // B button for LED flash
    if (bPressedD()) {
        led.toggle();
    }
    
    delay(50);
}

void retroCleanup() {
    retroActive = false;
    filterChain.clear();
    led.off();
    Serial.println("[RETRO] Mode exited");
}

// ============================================================
// Mode Definition
// ============================================================
DEFINE_CAMERA_MODE(retroModeDef, "Retro Cam", "Vintage Film Style Filters",
                   retroInit, retroLoop, retroCleanup);

// GameDef compatibility
GameDef retroMode = { "Retro", retroInit, retroLoop };
