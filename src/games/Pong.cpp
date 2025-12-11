#include "../core/Game.hpp"
#include "../core/Input.hpp"
#include <Adafruit_ST7735.h>

extern Adafruit_ST7735 tft;

// === CONFIG ===
#define COLOR_BG ST77XX_BLACK
#define COLOR_PADDLE ST77XX_WHITE
#define COLOR_BALL ST77XX_YELLOW
#define COLOR_TEXT ST77XX_CYAN

#define PADDLE_W 4
#define PADDLE_H 20
#define BALL_SIZE 4
#define SPEED_X 2
#define SPEED_Y 2

// === STRUCTS ===
struct Paddle { int x, y; };
struct Ball { int x, y, vx, vy; };

// === GLOBAL STATE ===
static Paddle leftPaddle, rightPaddle;
static Ball ball;
static int leftScore = 0, rightScore = 0;
static bool singlePlayer = true;
static bool modeSelected = false;

// === HELPERS ===
inline void resetBall() {
  ball.x = tft.width() / 2 - BALL_SIZE / 2;
  ball.y = tft.height() / 2 - BALL_SIZE / 2;
  ball.vx = (random(0, 2) == 0 ? SPEED_X : -SPEED_X);
  ball.vy = (random(0, 2) == 0 ? SPEED_Y : -SPEED_Y);
}

inline void drawPaddle(Paddle p) {
  tft.fillRect(p.x, p.y, PADDLE_W, PADDLE_H, COLOR_PADDLE);
}

inline void drawBall(Ball b) {
  tft.fillRect(b.x, b.y, BALL_SIZE, BALL_SIZE, COLOR_BALL);
}

inline void drawScore() {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(10, 2);
  tft.print(leftScore);
  tft.setCursor(tft.width() - 20, 2);
  tft.print(rightScore);
}

inline void resetGame() {
  leftScore = rightScore = 0;
  leftPaddle = {5, tft.height() / 2 - PADDLE_H / 2};
  rightPaddle = {tft.width() - PADDLE_W - 5, tft.height() / 2 - PADDLE_H / 2};
  resetBall();
  tft.fillScreen(COLOR_BG);
}

// === MODE SELECTION ===
void showModeSelect() {
  tft.fillScreen(COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(20, 30);
  tft.print("PONG");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 70);
  tft.print("Press A - Single Player");
  tft.setCursor(10, 90);
  tft.print("Press B - Multiplayer");

  while (!modeSelected) {
    if (aPressed()) {
      singlePlayer = true;
      modeSelected = true;
    } else if (bPressed()) {
      singlePlayer = false;
      modeSelected = true;
    }
    delay(50);
  }

  resetGame();
}

// === START FUNCTION ===
void startPong() {
  randomSeed(esp_random());
  modeSelected = false;
  showModeSelect();
}

// === LOOP FUNCTION ===
void loopPong() {
  // Erase previous frame (only the moving parts)
  tft.fillRect(0, 10, tft.width(), tft.height() - 10, COLOR_BG);

  // --- PLAYER Paddle (Always Joystick Controlled) ---
  Direction playerDir = readJoystickContinuous();
  if (playerDir == DIR_UP) leftPaddle.y -= 3;
  else if (playerDir == DIR_DOWN) leftPaddle.y += 3;

  // --- AI or Second Player Paddle ---
  if (singlePlayer) {
    // Simple AI for right paddle
    if (ball.y + BALL_SIZE / 2 > rightPaddle.y + PADDLE_H / 2 + 2) rightPaddle.y += 2;
    else if (ball.y + BALL_SIZE / 2 < rightPaddle.y + PADDLE_H / 2 - 2) rightPaddle.y -= 2;
  } else {
    // Multiplayer: use X/Y buttons
    if (xPressed()) rightPaddle.y -= 3;
    else if (yPressed()) rightPaddle.y += 3;
  }

  // Bound paddles
  leftPaddle.y = constrain(leftPaddle.y, 10, tft.height() - PADDLE_H);
  rightPaddle.y = constrain(rightPaddle.y, 10, tft.height() - PADDLE_H);

  // --- Move Ball ---
  ball.x += ball.vx;
  ball.y += ball.vy;

  // Wall collision
  if (ball.y <= 10 || ball.y >= tft.height() - BALL_SIZE) ball.vy *= -1;

  // Paddle collisions
  if (ball.x <= leftPaddle.x + PADDLE_W &&
      ball.y + BALL_SIZE >= leftPaddle.y &&
      ball.y <= leftPaddle.y + PADDLE_H) {
    ball.vx = abs(ball.vx);
  }
  if (ball.x + BALL_SIZE >= rightPaddle.x &&
      ball.y + BALL_SIZE >= rightPaddle.y &&
      ball.y <= rightPaddle.y + PADDLE_H) {
    ball.vx = -abs(ball.vx);
  }

  // Scoring
  if (ball.x <= 0) { rightScore++; resetBall(); }
  if (ball.x >= tft.width() - BALL_SIZE) { leftScore++; resetBall(); }

  // Draw everything
  drawPaddle(leftPaddle);
  drawPaddle(rightPaddle);
  drawBall(ball);
  drawScore();

  delay(15);
}

// === GAME DEF ===
GameDef pongGame = { "Pong", startPong, loopPong };
