#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "core/Display.hpp"
#include "core/Game.hpp"
#include "core/Input.hpp"
#include "core/Splash.hpp"
#include "core/Menu.hpp"

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

// game DECLARATION

extern GameDef snakeGame;

// ==============================================
//  Setup / Loop
// ==============================================
void setup() {
  Serial.begin(115200);
  tft.initR(INITR_GREENTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  registerGame(&snakeGame);


  showSplash();
  initMenu();
}

void loop() {
  handleMenuInput();
}