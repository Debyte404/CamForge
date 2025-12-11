#pragma once
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// === Your TFT pins ===
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
#define TFT_SCLK 18
#define TFT_MOSI 23

// Global display + SPI
inline SPIClass vspi(VSPI);
inline Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Call once at boot
inline void displayInit() {
  vspi.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);    // switch if your module needs GREENTAB/REDTAB
  tft.setRotation(1);           // 160x128 landscape
  tft.fillScreen(ST77XX_BLACK);
  tft.setSPISpeed(40000000);    // try 40 MHz; if unstable, reduce
}
