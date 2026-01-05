#include "../core/Game.hpp"
#include <Adafruit_ST7735.h>
extern Adafruit_ST7735 tft;

void startSnake() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("Snake Start!");
}

void loopSnake() {
  // your snake loop
}

GameDef snakeGame = {"Snake", startSnake, loopSnake};
