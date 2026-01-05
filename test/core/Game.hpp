#pragma once
#include <Arduino.h>

#define MAX_GAMES 10

struct GameDef {
  const char* name;
  void (*start)();
  void (*loop)();
};

// Declare the registry
extern GameDef* gameRegistry[MAX_GAMES];
extern int gameCount;

// Simple manual registration helper
inline void registerGame(GameDef* game) {
  if (gameCount < MAX_GAMES)
    gameRegistry[gameCount++] = game;
}
