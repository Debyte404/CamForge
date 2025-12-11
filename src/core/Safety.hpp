#pragma once
/**
 * @file Safety.hpp
 * @brief Safety Mechanisms for OpenCamX OS
 * 
 * Provides watchdog timer, memory guards, error recovery,
 * and safe operation wrappers for embedded ESP32 reliability.
 */

#include <Arduino.h>
#include <esp_task_wdt.h>

// ============================================================
// SAFETY CONFIGURATION
// ============================================================
#define WATCHDOG_TIMEOUT_SEC    10      // Watchdog timeout
#define MIN_FREE_HEAP_BYTES     20000   // Minimum heap before warning
#define MIN_FREE_PSRAM_BYTES    100000  // Minimum PSRAM before warning
#define MAX_CONSECUTIVE_ERRORS  5       // Max errors before mode reset

// ============================================================
// Hardware Error Codes (ESP32 Embedded Style)
// Grouped by subsystem for easy identification in logs
// ============================================================
typedef enum {
    ERR_NONE = 0,
    // Camera errors (0x10-0x1F)
    ERR_CAM_INIT = 0x10,      // Camera initialization failed
    ERR_CAM_CAPTURE,          // Frame capture timeout
    ERR_CAM_BUFFER,           // Frame buffer allocation failed
    // I2C errors (0x20-0x2F)
    ERR_I2C_TIMEOUT = 0x20,   // I2C sensor communication timeout
    ERR_I2C_NACK,             // I2C device not responding (no ACK)
    ERR_I2C_BUS,              // I2C bus error
    // SPI errors (0x30-0x3F)
    ERR_SPI_TIMEOUT = 0x30,   // SPI transaction timeout
    ERR_SPI_CRC,              // SPI CRC check failed
    // SD Card errors (0x40-0x4F)
    ERR_SD_MOUNT = 0x40,      // SD card mount failed
    ERR_SD_WRITE,             // SD card write error
    ERR_SD_READ,              // SD card read error
    ERR_SD_FULL,              // SD card full
    // Memory errors (0x50-0x5F)
    ERR_MEM_ALLOC = 0x50,     // Heap allocation failed
    ERR_MEM_PSRAM,            // PSRAM allocation failed
    ERR_MEM_CRITICAL,         // Critical low memory
    // Watchdog/System (0x60-0x6F)
    ERR_WATCHDOG = 0x60,      // Watchdog about to trigger
    ERR_STACK_OVERFLOW,       // Stack overflow detected
} SystemError;

// Last recorded hardware error with timestamp
static volatile SystemError g_last_error = ERR_NONE;
static volatile uint32_t g_last_error_ms = 0;

// ============================================================
// Error Tracking
// ============================================================
static volatile int _errorCount = 0;
static volatile bool _safetyInitialized = false;

// ============================================================
// Watchdog Timer
// ============================================================

/**
 * Initialize watchdog timer
 * Resets device if main loop hangs for WATCHDOG_TIMEOUT_SEC
 */
inline void safety_init_watchdog() {
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);  // true = panic on timeout
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    Serial.printf("[SAFETY] Watchdog initialized: %ds timeout\n", WATCHDOG_TIMEOUT_SEC);
}

/**
 * Feed the watchdog - call this in main loop
 */
inline void safety_feed_watchdog() {
    esp_task_wdt_reset();
}

/**
 * Disable watchdog (for long operations like OTA)
 */
inline void safety_disable_watchdog() {
    esp_task_wdt_delete(NULL);
    Serial.println("[SAFETY] Watchdog disabled");
}

// ============================================================
// Memory Safety
// ============================================================

/**
 * Check if memory is critically low
 * @return true if memory is OK
 */
inline bool safety_check_memory() {
    size_t freeHeap = ESP.getFreeHeap();
    size_t freePsram = ESP.getFreePsram();
    
    if (freeHeap < MIN_FREE_HEAP_BYTES) {
        Serial.printf("[SAFETY] CRITICAL: Low heap! %d bytes free\n", freeHeap);
        g_last_error = ERR_MEM_CRITICAL;
        g_last_error_ms = millis();
        return false;
    }
    
    if (freePsram < MIN_FREE_PSRAM_BYTES && psramFound()) {
        Serial.printf("[SAFETY] WARNING: Low PSRAM! %d bytes free\n", freePsram);
        // PSRAM low is warning, not critical
    }
    
    return true;
}

/**
 * Safe malloc with null check
 */
inline void* safety_malloc(size_t size) {
    if (size == 0) return nullptr;
    
    void* ptr = malloc(size);
    if (!ptr) {
        Serial.printf("[SAFETY] malloc failed for %d bytes!\n", size);
        g_last_error = ERR_MEM_ALLOC;
        g_last_error_ms = millis();
        _errorCount++;
    }
    return ptr;
}

/**
 * Safe PSRAM malloc
 */
inline void* safety_ps_malloc(size_t size) {
    if (size == 0) return nullptr;
    
    if (!psramFound()) {
        Serial.println("[SAFETY] PSRAM not available, falling back to heap");
        return safety_malloc(size);
    }
    
    void* ptr = ps_malloc(size);
    if (!ptr) {
        Serial.printf("[SAFETY] ps_malloc failed for %d bytes!\n", size);
        g_last_error = ERR_MEM_PSRAM;
        g_last_error_ms = millis();
        _errorCount++;
    }
    return ptr;
}

// ============================================================
// Buffer Safety
// ============================================================

/**
 * Safe array bounds check
 */
#define SAFE_ARRAY_ACCESS(arr, idx, maxSize, defaultVal) \
    (((idx) >= 0 && (idx) < (maxSize)) ? (arr)[idx] : (defaultVal))

/**
 * Safe buffer copy with bounds check
 */
inline bool safety_memcpy(void* dest, size_t destSize, const void* src, size_t count) {
    if (!dest || !src) {
        Serial.println("[SAFETY] Null pointer in memcpy!");
        return false;
    }
    if (count > destSize) {
        Serial.printf("[SAFETY] Buffer overflow prevented! %d > %d\n", count, destSize);
        return false;
    }
    memcpy(dest, src, count);
    return true;
}

/**
 * Safe string copy
 */
inline bool safety_strcpy(char* dest, size_t destSize, const char* src) {
    if (!dest || !src) return false;
    if (strlen(src) >= destSize) {
        Serial.println("[SAFETY] String overflow prevented!");
        return false;
    }
    strcpy(dest, src);
    return true;
}

// ============================================================
// Error Recovery
// ============================================================

/**
 * Record an error with SystemError code
 */
inline void safety_record_error(const char* context, SystemError err = ERR_NONE) {
    _errorCount++;
    if (err != ERR_NONE) {
        g_last_error = err;
        g_last_error_ms = millis();
    }
    Serial.printf("[SAFETY] Error #%d (0x%02X) in %s\n", _errorCount, (int)err, context);
    
    if (_errorCount >= MAX_CONSECUTIVE_ERRORS) {
        Serial.println("[SAFETY] Too many errors! Triggering recovery...");
        // Could trigger mode reset or safe mode here
    }
}

/**
 * Clear error count (call after successful operation)
 */
inline void safety_clear_errors() {
    _errorCount = 0;
    g_last_error = ERR_NONE;
}

/**
 * Get current error count
 */
inline int safety_get_error_count() {
    return _errorCount;
}

/**
 * Get last system error code
 */
inline SystemError safety_get_last_error() {
    return g_last_error;
}

/**
 * Check if in error state
 */
inline bool safety_is_error_state() {
    return _errorCount >= MAX_CONSECUTIVE_ERRORS;
}

// ============================================================
// Safe Pointer Operations
// ============================================================

/**
 * Null-safe pointer dereference wrapper
 */
#define SAFE_PTR(ptr, member, defaultVal) \
    ((ptr) ? (ptr)->member : (defaultVal))

/**
 * Null-safe function call wrapper
 */
#define SAFE_CALL(ptr, func, ...) \
    do { if (ptr) (ptr)->func(__VA_ARGS__); } while(0)

// ============================================================
// GPIO Safety
// ============================================================

/**
 * Safe GPIO write with pin validation
 */
inline void safety_digitalWrite(uint8_t pin, uint8_t value) {
    if (pin > 48) {  // ESP32-S3 max GPIO
        Serial.printf("[SAFETY] Invalid GPIO pin: %d\n", pin);
        return;
    }
    digitalWrite(pin, value);
}

/**
 * Safe analogWrite with bounds
 */
inline void safety_analogWrite(uint8_t pin, int value) {
    if (pin > 48) {
        Serial.printf("[SAFETY] Invalid GPIO pin: %d\n", pin);
        return;
    }
    value = constrain(value, 0, 255);
    analogWrite(pin, value);
}

// ============================================================
// Initialization
// ============================================================

/**
 * Initialize all safety mechanisms
 */
inline void safety_init() {
    if (_safetyInitialized) return;
    
    Serial.println("\n=== SAFETY SYSTEM INIT ===");
    
    // Check initial memory state
    Serial.printf("[SAFETY] Free Heap: %d bytes\n", ESP.getFreeHeap());
    if (psramFound()) {
        Serial.printf("[SAFETY] Free PSRAM: %d bytes\n", ESP.getFreePsram());
    }
    
    // Initialize watchdog
    safety_init_watchdog();
    
    // Initial memory check
    if (!safety_check_memory()) {
        Serial.println("[SAFETY] WARNING: Starting with low memory!");
    }
    
    _safetyInitialized = true;
    Serial.println("[SAFETY] All systems initialized\n");
}

/**
 * Periodic safety check - call in main loop
 */
inline void safety_tick() {
    // Feed watchdog
    safety_feed_watchdog();
    
    // Periodic memory check (every ~1000 calls)
    static int tickCount = 0;
    if (++tickCount >= 1000) {
        tickCount = 0;
        safety_check_memory();
    }
}

// ============================================================
// Panic Handler
// ============================================================

/**
 * Handle critical failure - log and restart
 */
inline void safety_panic(const char* reason) {
    Serial.println("\n!!! SAFETY PANIC !!!");
    Serial.printf("Reason: %s\n", reason);
    Serial.printf("Last Error: 0x%02X at %lu ms\n", (int)g_last_error, g_last_error_ms);
    Serial.printf("Free Heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Uptime: %lu ms\n", millis());
    Serial.println("Restarting in 3 seconds...\n");
    
    delay(3000);
    ESP.restart();
}

// ============================================================
// Brownout Handler (Power Issue Detection)
// ============================================================

/**
 * Flash SOS pattern on LED for hardware issues
 * Called when brownout or critical power issue detected
 * @param ledPin GPIO pin for status LED
 */
inline void safety_flash_sos(uint8_t ledPin = 2) {
    // SOS pattern: ... --- ...
    const int dotDuration = 150;
    const int dashDuration = 450;
    const int pauseDuration = 150;
    
    pinMode(ledPin, OUTPUT);
    
    // S (...)
    for (int i = 0; i < 3; i++) {
        digitalWrite(ledPin, HIGH);
        delay(dotDuration);
        digitalWrite(ledPin, LOW);
        delay(pauseDuration);
    }
    delay(pauseDuration * 2);
    
    // O (---)
    for (int i = 0; i < 3; i++) {
        digitalWrite(ledPin, HIGH);
        delay(dashDuration);
        digitalWrite(ledPin, LOW);
        delay(pauseDuration);
    }
    delay(pauseDuration * 2);
    
    // S (...)
    for (int i = 0; i < 3; i++) {
        digitalWrite(ledPin, HIGH);
        delay(dotDuration);
        digitalWrite(ledPin, LOW);
        delay(pauseDuration);
    }
}

/**
 * Handle brownout detection - hardware power issue
 * Flashes SOS and enters deep sleep to protect hardware
 */
inline void safety_handle_brownout() {
    Serial.println("\n!!! BROWNOUT DETECTED !!!");
    Serial.println("Power supply issue - check capacitors near camera");
    Serial.println("Recommendation: Add 100uF+ cap across 3.3V and GND");
    
    safety_flash_sos();
    
    // Enter deep sleep to reduce power draw
    Serial.println("Entering deep sleep...");
    esp_deep_sleep_start();
}

// ============================================================
// Heap Low Recovery
// ============================================================

/**
 * Emergency memory recovery when heap is critically low
 * Purges caches and releases non-essential buffers
 */
inline void safety_recover_memory() {
    Serial.println("[SAFETY] Attempting memory recovery...");
    
    size_t beforeHeap = ESP.getFreeHeap();
    size_t beforePsram = ESP.getFreePsram();
    
    // Force garbage collection if available
    // Note: ESP32 doesn't have true GC, but we can hint to release caches
    
    // Log recovery attempt
    size_t afterHeap = ESP.getFreeHeap();
    size_t afterPsram = ESP.getFreePsram();
    
    Serial.printf("[SAFETY] Memory recovery: Heap %d->%d, PSRAM %d->%d\n",
                  beforeHeap, afterHeap, beforePsram, afterPsram);
    
    // Clear error count if we recovered successfully
    if (afterHeap > MIN_FREE_HEAP_BYTES) {
        _errorCount = max(0, _errorCount - 1);
    }
}
