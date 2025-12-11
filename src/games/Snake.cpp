#include "../core/Game.hpp"
#include "../core/Input.hpp"
#include <Adafruit_ST7735.h>

extern Adafruit_ST7735 tft;

// === Snake Constants ===
#define GRID_SIZE 8
#define GRID_W (tft.width() / GRID_SIZE)
#define GRID_H (tft.height() / GRID_SIZE)

#define COLOR_BG ST77XX_BLACK
#define COLOR_SNAKE ST77XX_GREEN
#define COLOR_FOOD ST77XX_RED

struct Point { int x, y; };

static Point snake[100];
static int snakeLength = 3;
static Point food;
static int dirX = 1, dirY = 0;     // movement direction
static int lastDirX = 1, lastDirY = 0; // last valid direction (for reversal check)
static unsigned long lastMove = 0;
static int speed = 150;
static bool gameOver = false;

// === Helpers ===
inline bool collide(Point a, Point b) { return a.x == b.x && a.y == b.y; }

inline void drawCell(Point p, uint16_t color) {
  tft.fillRect(p.x * GRID_SIZE, p.y * GRID_SIZE, GRID_SIZE - 1, GRID_SIZE - 1, color);
}

inline void placeFood() {
  food.x = random(0, GRID_W);
  food.y = random(1, GRID_H);
}

inline void resetSnake() {
  snakeLength = 3;
  dirX = 1; dirY = 0;
  lastDirX = dirX; lastDirY = dirY;
  for (int i = 0; i < snakeLength; i++) {
    snake[i] = {3 - i, 3};
  }
  placeFood();
  gameOver = false;
  speed = 150;
  tft.fillScreen(COLOR_BG);
}

// === Start Function ===
void startSnake() {
  randomSeed(esp_random());
  resetSnake();
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(20, 5);
  tft.print("SNAKE!");
  delay(500);
  tft.fillScreen(COLOR_BG);
}

// === Loop Function ===
void loopSnake() {
  if (gameOver) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(15, 40);
    tft.print("Game Over");
    tft.setTextSize(1);
    tft.setCursor(20, 65);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Press BACK");
    return;
  }

  // === Direction Input ===
  Direction dir = readJoystickStateChange();

  // Prevent 180° reverse direction in same frame
  if (dir == DIR_UP    && !(lastDirY ==  1)) { dirX = 0; dirY = -1; }
  else if (dir == DIR_DOWN  && !(lastDirY == -1)) { dirX = 0; dirY =  1; }
  else if (dir == DIR_LEFT  && !(lastDirX ==  1)) { dirX = -1; dirY = 0; }
  else if (dir == DIR_RIGHT && !(lastDirX == -1)) { dirX =  1; dirY = 0; }

  // Movement timing
  if (millis() - lastMove < speed) return;
  lastMove = millis();

  Point newHead = { snake[0].x + dirX, snake[0].y + dirY };

  // === Wrap-around teleport ===
  if (newHead.x < 0) newHead.x = GRID_W - 1;
  if (newHead.y < 0) newHead.y = GRID_H - 1;
  if (newHead.x >= GRID_W) newHead.x = 0;
  if (newHead.y >= GRID_H) newHead.y = 0;

  // === Self-collision check ===
  for (int i = 0; i < snakeLength; i++) {
    if (collide(newHead, snake[i])) {
      gameOver = true;
      return;
    }
  }

  // === Move snake ===
  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  snake[0] = newHead;

  // === Food eaten ===
  if (collide(snake[0], food)) {
    if (snakeLength < 99) snakeLength++;
    speed = max(50, speed - 5);
    placeFood();
  }

  // === Draw frame ===
  tft.fillScreen(COLOR_BG);
  drawCell(food, COLOR_FOOD);
  for (int i = 0; i < snakeLength; i++) {
    drawCell(snake[i], COLOR_SNAKE);
  }

  // Remember last direction
  lastDirX = dirX;
  lastDirY = dirY;
}

// === Game Definition ===
GameDef snakeGame = { "Snake", startSnake, loopSnake };
