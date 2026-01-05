#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "Game.hpp"
#include "Input.hpp"

#define ST77XX_DARKGREY tft.color565(64, 64, 64)
#define VISIBLE_ITEMS 4

extern Adafruit_ST7735 tft;
extern GameDef* gameRegistry[MAX_GAMES];
extern int gameCount;

static int selectedGame = 0;
static int scrollOffset = 0;

inline void drawMenuFrame() {
  tft.fillScreen(ST77XX_BLACK);

  // Header
  const char* title = "Select Game";
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  int16_t xHeader = (tft.width() - (int)strlen(title) * 12) / 2;
  tft.setCursor(xHeader, 6);
  tft.print(title);

  // Frame border
  tft.drawRect(2, 2, tft.width() - 4, tft.height() - 4, ST77XX_DARKGREY);

  // Footer
  const char* footer = "w/ caffeine by Debyte";
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_DARKGREY);
  int16_t xFooter = (tft.width() - (int)strlen(footer) * 6) / 2;
  tft.setCursor(xFooter, tft.height() - 15);
  tft.print(footer);
}

inline void drawMenuItems() {
  // clear menu list area (keep header/footer)
  tft.fillRect(5, 25, tft.width() - 10, tft.height() - 40, ST77XX_BLACK);

  const int itemHeight = 17;
  const int startY = 35;
  tft.setTextSize(2);

  const int visible = min(VISIBLE_ITEMS, gameCount);
  for (int i = 0; i < visible; i++) {
    const int gameIndex = i + scrollOffset;
    if (gameIndex >= gameCount) break;

    const int y = startY + i * itemHeight;

    if (gameIndex == selectedGame) {
      // highlight bar behind text
      tft.fillRect(10, y - 2, tft.width() - 20, itemHeight + 1, ST77XX_YELLOW);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }

    int16_t x = (tft.width() - (int)strlen(gameRegistry[gameIndex]->name) * 12) / 2;
    tft.setCursor(x, y);
    tft.print(gameRegistry[gameIndex]->name);
  }
}

inline void ensureSelectionVisible(bool wrapped) {
  // If few items, pin to 0
  if (gameCount <= VISIBLE_ITEMS) {
    scrollOffset = 0;
    return;
  }

  if (wrapped) {
    // When wrapping, snap window to start/end
    if (selectedGame == 0) {
      scrollOffset = 0;
    } else if (selectedGame == gameCount - 1) {
      scrollOffset = max(0, gameCount - VISIBLE_ITEMS);
    }
    return;
  }

  // Non-wrapped motion: adjust only as needed
  if (selectedGame < scrollOffset) {
    scrollOffset = selectedGame;
  } else if (selectedGame >= scrollOffset + VISIBLE_ITEMS) {
    scrollOffset = selectedGame - VISIBLE_ITEMS + 1;
  }
}

inline void moveSelection(int delta) {
  if (gameCount == 0) return;

  const int prev = selectedGame;
  // wrap selection
  selectedGame = (selectedGame + delta + gameCount) % gameCount;

  const bool wrapped =
      (delta < 0 && prev == 0) ||
      (delta > 0 && prev == gameCount - 1);

  ensureSelectionVisible(wrapped);
  drawMenuItems();
}

inline void handleMenuInput() {
  if (gameCount == 0) return;

  Direction dir = readJoystickStateChange();

  if (dir == DIR_UP) {
    moveSelection(-1);
  } else if (dir == DIR_DOWN) {
    moveSelection(+1);
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

  delay(80); // debounce / reduce redraws
}

inline void initMenu() {
  // keep selectedGame in-bounds if registry changed
  if (selectedGame >= gameCount) selectedGame = max(0, gameCount - 1);
  if (gameCount <= VISIBLE_ITEMS) scrollOffset = 0;
  else ensureSelectionVisible(false);

  drawMenuFrame();
  drawMenuItems();
}
