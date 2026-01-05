#pragma once
/**
 * @file IRLed.hpp
 * @brief IR LED Driver for Night Vision Mode
 * 
 * PWM-controlled infrared LEDs with adjustable intensity
 */

#include <Arduino.h>

// IR LED Pin (GPIO 45 - strapping pin, safe as output after boot)
// NOTE: GPIO 2 is reserved for TFT Display DC signal
#define IR_LED_PIN 45

class IRLedDriver {
private:
    uint8_t _pin;
    bool _active = false;
    uint8_t _intensity = 200;  // 0-255
    
public:
    IRLedDriver(uint8_t pin = IR_LED_PIN) : _pin(pin) {}
    
    void init() {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        _active = false;
        Serial.printf("[IR] Initialized on GPIO %d\n", _pin);
    }
    
    void enable() {
        analogWrite(_pin, _intensity);
        _active = true;
    }
    
    void disable() {
        digitalWrite(_pin, LOW);
        _active = false;
    }
    
    void toggle() {
        if (_active) disable();
        else enable();
    }
    
    void setIntensity(uint8_t level) {
        _intensity = level;
        if (_active) {
            analogWrite(_pin, _intensity);
        }
    }
    
    bool isActive() const { return _active; }
    uint8_t getIntensity() const { return _intensity; }
};

// Global IR LED instance
inline IRLedDriver irLed;
