#include "../core/Game.hpp"
#include <Adafruit_ST7735.h>
extern Adafruit_ST7735 tft;

void startOne() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("1");
}

void loopOne() {
  // your Pong loop
}

GameDef one = {"1", startOne, loopOne};
