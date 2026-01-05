#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "Game.hpp"
#include "Input.hpp"

#define ST77XX_DARKGREY tft.color565(64, 64, 64)

extern Adafruit_ST7735 tft;
extern GameDef* gameRegistry[MAX_GAMES];
extern int gameCount;

static int selectedGame = 0;
static uint8_t pulse = 0;
static int8_t pulseDir = 3;
static unsigned long lastUpdate = 0;

inline void drawMenuFrame() {
  tft.fillScreen(ST77XX_BLACK);

  // Header
  const char* title = "Select Game";
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  int16_t xHeader = (tft.width() - strlen(title) * 12) / 2;
  tft.setCursor(xHeader, 5);
  tft.print(title);

  // Frame border
  tft.drawRect(2, 2, tft.width() - 4, tft.height() - 4, ST77XX_DARKGREY);

  // Footer
  const char* footer = "w/ caffeine by Debyte";
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_DARKGREY);
  int16_t xFooter = (tft.width() - strlen(footer) * 6) / 2;
  tft.setCursor(xFooter, tft.height() - 10);
  tft.print(footer);
}

inline void drawMenuItems() {
  int itemHeight = 20;
  int startY = 35;
  tft.setTextSize(2);

  for (int i = 0; i < gameCount; i++) {
    int y = startY + i * itemHeight;

    if (i == selectedGame) {
      uint16_t color = tft.color565(255, 255, 0); // static yellow
      tft.fillRect(10, y - 2, tft.width() - 20, itemHeight + 2, color);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.fillRect(10, y - 2, tft.width() - 20, itemHeight + 2, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE);
    }

    int16_t x = (tft.width() - strlen(gameRegistry[i]->name) * 12) / 2;
    tft.setCursor(x, y);
    tft.print(gameRegistry[i]->name);
  }
}

inline void handleMenuInput() {
  if (gameCount == 0) return;
  Direction dir = readJoystickStateChange();

   // === DEBUG TEST ===
  if (dir == DIR_UP) {
    Serial.println("Joystick moved UP");
  } else if (dir == DIR_DOWN) {
    Serial.println("Joystick moved DOWN");
  } else if (dir == DIR_LEFT) {
    Serial.println("Joystick moved LEFT");
  } else if (dir == DIR_RIGHT) {
    Serial.println("Joystick moved RIGHT");
  }

  if (selectPressed()) {
    Serial.println("SELECT pressed");
  }
  if (aPressed()) {
    Serial.println("A pressed");
  }
  if (bPressed()) {
    Serial.println("B pressed");
  }
  if (xPressed()) {
    Serial.println("X pressed");
  }
  if (yPressed()) {
    Serial.println("Y pressed");
  }
  if (backPressed()) {
    Serial.println("BACK pressed");
  }

  if (dir == DIR_UP) {
    selectedGame = (selectedGame - 1 + gameCount) % gameCount;
    drawMenuItems();  // Only redraw items
  } else if (dir == DIR_DOWN) {
    selectedGame = (selectedGame + 1) % gameCount;
    drawMenuItems();
  }

  if (selectPressed() || aPressed()) {
    tft.fillScreen(ST77XX_BLACK);
    gameRegistry[selectedGame]->start();

    while (true) {
      gameRegistry[selectedGame]->loop();
      if (backPressed()) {
        drawMenuFrame();
        drawMenuItems();
        break;
      }
    }
  }

  // Optional small pulse tint, throttled to ~20 fps
  if (millis() - lastUpdate > 50) {
    pulse += pulseDir;
    if (pulse >= 60 || pulse <= 0) pulseDir = -pulseDir;

    int y = 35 + selectedGame * 20;
    uint16_t color = tft.color565(255, 255 - pulse, 0);
    tft.fillRect(10, y - 2, tft.width() - 20, 20, color);
    tft.setTextColor(ST77XX_BLACK);
    tft.setTextSize(2);
    int16_t x = (tft.width() - strlen(gameRegistry[selectedGame]->name) * 12) / 2;
    tft.setCursor(x, y);
    tft.print(gameRegistry[selectedGame]->name);
    lastUpdate = millis();
  }
}

inline void initMenu() {
  drawMenuFrame();
  drawMenuItems();
}
