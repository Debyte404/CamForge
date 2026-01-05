#pragma once
/**
 * @file LED.hpp
 * @brief LED Flashlight Driver for OpenCamX
 * 
 * Simple GPIO control for white LED flashlight
 */

#include <Arduino.h>

// LED Flashlight Pin (GPIO 38 - avoids conflicts with camera/display)
// NOTE: GPIO 4 is reserved for Camera I2C SDA
#define LED_FLASH_PIN 38

class LEDDriver {
private:
    uint8_t _pin;
    bool _state = false;
    uint8_t _brightness = 255;

public:
    LEDDriver(uint8_t pin = LED_FLASH_PIN) : _pin(pin) {}
    
    void init() {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        _state = false;
        Serial.printf("[LED] Initialized on GPIO %d\n", _pin);
    }
    
    void on() {
        analogWrite(_pin, _brightness);
        _state = true;
    }
    
    void off() {
        digitalWrite(_pin, LOW);
        _state = false;
    }
    
    void toggle() {
        if (_state) off();
        else on();
    }
    
    void setBrightness(uint8_t level) {
        _brightness = level;
        if (_state) {
            analogWrite(_pin, _brightness);
        }
    }
    
    bool isOn() const { return _state; }
    uint8_t getBrightness() const { return _brightness; }
};

// Global LED instance
inline LEDDriver led;
