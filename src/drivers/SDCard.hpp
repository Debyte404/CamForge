#pragma once
/**
 * @file SDCard.hpp
 * @brief SD Card Driver for OpenCamX POV Recording
 * 
 * FATFS-based SD card access for video clip storage
 */

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// ============================================================
// SD Card Pins (Shared SPI Bus - Corrected GPIO Map)
// ============================================================
// Shares SPI bus with TFT Display (MOSI:39, SCK:40, MISO:41)
// Uses separate CS pin (GPIO 46) to avoid conflicts
// IMPORTANT: GPIO 46 is a strapping pin but safe for output use after boot
// ============================================================
#define SD_CS_PIN   46   // Chip Select (was 15 - conflicted with camera)
#define SD_MOSI_PIN 39   // Master Out (shared with TFT)
#define SD_MISO_PIN 41   // Master In (shared with TFT)  
#define SD_CLK_PIN  40   // SPI Clock (shared with TFT)

class SDCardDriver {
private:
    bool _mounted = false;
    uint64_t _totalBytes = 0;
    uint64_t _usedBytes = 0;

public:
    /**
     * Initialize SD card
     * @return true if mounted successfully
     */
    bool init() {
        SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
        
        if (!SD.begin(SD_CS_PIN)) {
            Serial.println("[SD] Mount failed!");
            return false;
        }
        
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            Serial.println("[SD] No card detected");
            return false;
        }
        
        _totalBytes = SD.totalBytes();
        _usedBytes = SD.usedBytes();
        _mounted = true;
        
        Serial.printf("[SD] Mounted: %s, Total: %lluMB, Used: %lluMB\n",
            cardType == CARD_MMC ? "MMC" :
            cardType == CARD_SD ? "SDSC" :
            cardType == CARD_SDHC ? "SDHC" : "Unknown",
            _totalBytes / (1024 * 1024),
            _usedBytes / (1024 * 1024)
        );
        
        // Create recordings directory if not exists
        if (!SD.exists("/recordings")) {
            SD.mkdir("/recordings");
        }
        
        return true;
    }
    
    /**
     * Open a file for writing
     */
    File openFile(const char* path, const char* mode = FILE_WRITE) {
        return SD.open(path, mode);
    }
    
    /**
     * Check if file exists
     */
    bool exists(const char* path) {
        return SD.exists(path);
    }
    
    /**
     * Delete a file
     */
    bool remove(const char* path) {
        return SD.remove(path);
    }
    
    /**
     * Get free space in bytes
     */
    uint64_t getFreeBytes() const {
        return _totalBytes - _usedBytes;
    }
    
    /**
     * Generate unique filename with timestamp
     * @deprecated Use generateFilenameSafe() to avoid heap fragmentation
     */
    String generateFilename(const char* prefix = "VID", const char* ext = "avi") {
        static uint32_t counter = 0;
        counter++;
        return String("/recordings/") + prefix + "_" + String(millis()) + "_" + String(counter) + "." + ext;
    }
    
    /**
     * Generate unique filename into caller-provided buffer (NO HEAP ALLOCATION)
     * Use this instead of generateFilename() to prevent heap fragmentation
     * on long-running embedded devices.
     * @param out_buf Output buffer (min 48 bytes for safety)
     * @param buf_len Size of output buffer
     * @param prefix Filename prefix (default: "VID")
     * @param ext File extension (default: "avi")
     * @return true if path fits in buffer
     */
    bool generateFilenameSafe(char* out_buf, size_t buf_len, 
                              const char* prefix = "VID", const char* ext = "avi") {
        static uint32_t s_file_counter = 0;  // s_ prefix for static per embedded convention
        s_file_counter++;
        int written = snprintf(out_buf, buf_len, "/recordings/%s_%lu_%lu.%s",
                               prefix, (unsigned long)millis(), (unsigned long)s_file_counter, ext);
        return written > 0 && (size_t)written < buf_len;
    }
    
    bool isMounted() const { return _mounted; }
    uint64_t getTotalBytes() const { return _totalBytes; }
    uint64_t getUsedBytes() const { return _usedBytes; }
};

// Global SD card instance
inline SDCardDriver sdCard;
