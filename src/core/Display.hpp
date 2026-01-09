#pragma once
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ============================================================
// TFT Display Pins (Shared SPI Bus - Corrected GPIO Map)
// ============================================================
// Shares SPI bus with SD Card (MOSI:39, SCK:40, MISO:41)
// RST set to -1 (not connected) - use software reset
// ============================================================
#define TFT_CS   42   // Chip Select (was 5 - conflicted with I2C)
#define TFT_DC   2    // Data/Command
#define TFT_RST  -1   // Reset (not connected, use software reset)
#define TFT_SCLK 40   // SPI Clock (shared)
#define TFT_MOSI 39   // Master Out (shared)

// ESP32-S3 uses FSPI, fall back to HSPI if not defined
#ifndef FSPI
  #define FSPI 1
#endif

// Global display + SPI (C++17 inline variables)
inline SPIClass vspi(FSPI);
inline Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Call once at boot
inline void displayInit() {
  vspi.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_GREENTAB);    // switch if your module needs GREENTAB/REDTAB
  tft.setRotation(1);           // 160x128 landscape
  tft.fillScreen(ST77XX_BLACK);
  tft.setSPISpeed(40000000);    // try 40 MHz; if unstable, reduce
}
