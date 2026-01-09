#pragma once
#include <Arduino.h>

// =======================================================
// === Pin Definitions (adjust for your wiring)
// =======================================================
#define JOY_X_PIN    34
#define JOY_Y_PIN    35

#define BTN_SELECT   19   // Joystick push button (SW)
#define BTN_A        32
#define BTN_B        33
#define BTN_X        26
#define BTN_Y        27
#define BTN_BACK     25

// =======================================================
// === Joystick Calibration & Deadzone
// =======================================================
#define JOY_CENTER   2048
#define JOY_DEADZONE 400

// =======================================================
// === Direction Enum
// =======================================================
enum Direction {
  DIR_NONE,
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
};

// =======================================================
// === Input Initialization
// =======================================================
inline void initInput() {
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
}

// =======================================================
// === Joystick Continuous Read
// =======================================================
inline Direction readJoystickContinuous() {
  int x = analogRead(JOY_X_PIN);
  int y = analogRead(JOY_Y_PIN);

  if (y < JOY_CENTER - JOY_DEADZONE) return DIR_UP;
  if (y > JOY_CENTER + JOY_DEADZONE) return DIR_DOWN;
  if (x < JOY_CENTER - JOY_DEADZONE) return DIR_LEFT;
  if (x > JOY_CENTER + JOY_DEADZONE) return DIR_RIGHT;
  return DIR_NONE;
}

// =======================================================
// === Joystick State Change Read (single-trigger)
// =======================================================
inline Direction readJoystickStateChange() {
  static Direction lastDir = DIR_NONE;
  Direction current = readJoystickContinuous();

  if (current != lastDir && current != DIR_NONE) {
    lastDir = current;
    return current;
  }

  if (current == DIR_NONE) lastDir = DIR_NONE; // reset when released
  return DIR_NONE;
}

// =======================================================
// === Button Reads
// =======================================================
inline bool isPressed(uint8_t pin) { return digitalRead(pin) == LOW; }

inline bool selectPressed() { return isPressed(BTN_SELECT); }
inline bool aPressed()      { return isPressed(BTN_A); }
inline bool bPressed()      { return isPressed(BTN_B); }
inline bool xPressed()      { return isPressed(BTN_X); }
inline bool yPressed()      { return isPressed(BTN_Y); }
inline bool backPressed()   { return isPressed(BTN_BACK); }

// =======================================================
// === Optional Debounced Press
// =======================================================
inline bool debouncedPress(uint8_t pin, unsigned long debounceDelay = 150) {
  static unsigned long lastPressTime = 0;
  if (isPressed(pin) && (millis() - lastPressTime > debounceDelay)) {
    lastPressTime = millis();
    return true;
  }
  return false;
}
