#pragma once
/**
 * @file ModeBase.hpp
 * @brief Extended mode interface for OpenCamX camera modes
 * 
 * Builds on GameDef pattern but adds camera-specific lifecycle
 * methods like preview, capture, and cleanup.
 */

#include <Arduino.h>
#include "Game.hpp"

// Mode type identifiers
enum ModeType {
    MODE_GAME,      // Traditional game
    MODE_CAMERA,    // Camera mode (uses camera driver)
    MODE_TOOL       // System utility
};

/**
 * Extended mode definition for camera modes
 */
struct ModeDef {
    const char* name;
    const char* description;
    ModeType    type;
    
    void (*init)();     // Called once when mode is selected
    void (*loop)();     // Called repeatedly while mode is active
    void (*cleanup)();  // Called when exiting mode
    
    // Camera-specific callbacks (optional, can be nullptr)
    void (*onCapture)();    // Called when capture button pressed
    void (*onToggle)();     // Called when toggle button pressed
};

// Convert ModeDef to GameDef for compatibility with existing menu
inline GameDef modeToGame(ModeDef* mode) {
    return { mode->name, mode->init, mode->loop };
}

/**
 * Simple macro to define a camera mode
 */
#define DEFINE_CAMERA_MODE(varName, modeName, desc, initFn, loopFn, cleanupFn) \
    ModeDef varName = { modeName, desc, MODE_CAMERA, initFn, loopFn, cleanupFn, nullptr, nullptr }

#define DEFINE_CAMERA_MODE_FULL(varName, modeName, desc, initFn, loopFn, cleanupFn, captureFn, toggleFn) \
    ModeDef varName = { modeName, desc, MODE_CAMERA, initFn, loopFn, cleanupFn, captureFn, toggleFn }
