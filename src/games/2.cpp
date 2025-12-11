#include "../core/Game.hpp"
#include <Adafruit_ST7735.h>
extern Adafruit_ST7735 tft;

void startTwo() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("2");
}

void loopTwo() {
  // your Pong loop
}

GameDef two = {"2", startTwo, loopTwo};
