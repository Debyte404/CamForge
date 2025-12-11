#pragma once
/**
 * @file Camera.hpp
 * @brief ESP32-S3 Camera Driver for OpenCamX OS
 * 
 * Wrapper around esp32-camera library with simplified API
 * for frame capture, resolution control, and format switching.
 * 
 * MEMORY ARCHITECTURE:
 * - 400KB Internal SRAM → Filter processing, core logic (fast)
 * - 8MB PSRAM → Frame buffers, PyForge mods (large storage)
 * 
 * Frame buffers are allocated in PSRAM via CAMERA_FB_IN_PSRAM.
 * Filters process IN-PLACE on PSRAM buffers to avoid copies.
 */

#include <Arduino.h>
#include "esp_camera.h"

// ============================================================
// PSRAM Configuration for Frame Buffers
// ============================================================
#define FRAME_BUFFER_COUNT  2   // Double buffering in PSRAM
#define JPEG_QUALITY        12  // 0-63, lower = better quality

// ============================================================
// Camera Pin Definitions (Corrected GPIO Map - No Conflicts)
// ============================================================
// IMPORTANT: Avoids strapping pins (0, 45, 46) and PSRAM pins (35, 36, 37)
// Camera uses DVP parallel interface, separate from SPI bus (Display/SD)
//
// GPIO Assignment:
//   D0-D7: 1, 6, 8, 9, 10, 11, 12, 13 (Parallel data bus)
//   Control: XCLK:14, PCLK:21, VSYNC:47, HSYNC:48
//   I2C (SCCB): SDA:4, SCL:5 (shared with peripherals)
// ============================================================
#define CAM_PIN_PWDN    -1   // Power down (not used)
#define CAM_PIN_RESET   -1   // Reset (not used)
#define CAM_PIN_XCLK    14   // External clock (was 15)
#define CAM_PIN_SIOD    4    // I2C SDA (SCCB)
#define CAM_PIN_SIOC    5    // I2C SCL (SCCB)
#define CAM_PIN_D7      13   // Data bit 7
#define CAM_PIN_D6      12   // Data bit 6
#define CAM_PIN_D5      11   // Data bit 5
#define CAM_PIN_D4      10   // Data bit 4
#define CAM_PIN_D3      9    // Data bit 3
#define CAM_PIN_D2      8    // Data bit 2
#define CAM_PIN_D1      6    // Data bit 1
#define CAM_PIN_D0      1    // Data bit 0
#define CAM_PIN_VSYNC   47   // Vertical sync (was 6)
#define CAM_PIN_HREF    48   // Horizontal sync (was 7)
#define CAM_PIN_PCLK    21   // Pixel clock (was 13)

// ============================================================
// Resolution Presets (Max 1600p / UXGA)
// ============================================================
enum CamResolution {
    CAM_RES_QQVGA = FRAMESIZE_QQVGA,  // 160x120
    CAM_RES_QVGA  = FRAMESIZE_QVGA,   // 320x240
    CAM_RES_VGA   = FRAMESIZE_VGA,    // 640x480
    CAM_RES_SVGA  = FRAMESIZE_SVGA,   // 800x600
    CAM_RES_XGA   = FRAMESIZE_XGA,    // 1024x768
    CAM_RES_HD    = FRAMESIZE_HD,     // 1280x720
    CAM_RES_SXGA  = FRAMESIZE_SXGA,   // 1280x1024
    CAM_RES_UXGA  = FRAMESIZE_UXGA,   // 1600x1200 (MAX)
};

// ============================================================
// Camera Frame Wrapper
// ============================================================
struct CamFrame {
    uint8_t* data;
    size_t   len;
    int      width;
    int      height;
    pixformat_t format;
    
    bool valid() const { return data != nullptr && len > 0; }
};

// ============================================================
// Camera Driver Class
// ============================================================
class CameraDriver {
private:
    bool _initialized = false;
    camera_fb_t* _currentFrame = nullptr;
    CamResolution _resolution = CAM_RES_VGA;
    pixformat_t _format = PIXFORMAT_JPEG;

public:
    /**
     * Initialize camera with default settings
     * Frame buffers are allocated in PSRAM (8MB available)
     * @return true on success
     */
    bool init() {
        if (_initialized) return true;
        
        // Check PSRAM availability
        if (!psramFound()) {
            Serial.println("[CAM] WARNING: PSRAM not found! Frame buffers will use internal RAM.");
        } else {
            Serial.printf("[CAM] PSRAM available: %d bytes\n", ESP.getPsramSize());
        }
        
        camera_config_t config;
        config.ledc_channel = LEDC_CHANNEL_0;
        config.ledc_timer   = LEDC_TIMER_0;
        config.pin_d0       = CAM_PIN_D0;
        config.pin_d1       = CAM_PIN_D1;
        config.pin_d2       = CAM_PIN_D2;
        config.pin_d3       = CAM_PIN_D3;
        config.pin_d4       = CAM_PIN_D4;
        config.pin_d5       = CAM_PIN_D5;
        config.pin_d6       = CAM_PIN_D6;
        config.pin_d7       = CAM_PIN_D7;
        config.pin_xclk     = CAM_PIN_XCLK;
        config.pin_pclk     = CAM_PIN_PCLK;
        config.pin_vsync    = CAM_PIN_VSYNC;
        config.pin_href     = CAM_PIN_HREF;
        config.pin_sccb_sda = CAM_PIN_SIOD;
        config.pin_sccb_scl = CAM_PIN_SIOC;
        config.pin_pwdn     = CAM_PIN_PWDN;
        config.pin_reset    = CAM_PIN_RESET;
        config.xclk_freq_hz = 20000000;
        config.pixel_format = _format;
        config.frame_size   = (framesize_t)_resolution;
        config.jpeg_quality = JPEG_QUALITY;
        config.fb_count     = FRAME_BUFFER_COUNT;
        
        // Use PSRAM for frame buffers (saves internal RAM for filters)
        config.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
        config.grab_mode    = CAMERA_GRAB_LATEST;
        
        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            Serial.printf("[CAM] Init failed: 0x%x\n", err);
            return false;
        }
        
        _initialized = true;
        Serial.println("[CAM] Initialized successfully");
        Serial.printf("[CAM] Free heap: %d, Free PSRAM: %d\n", 
                      ESP.getFreeHeap(), ESP.getFreePsram());
        return true;
    }
    
    /**
     * Capture a single frame
     * @return CamFrame with image data (must call releaseFrame after use)
     */
    CamFrame captureFrame() {
        CamFrame frame = {nullptr, 0, 0, 0, PIXFORMAT_JPEG};
        
        if (!_initialized) return frame;
        
        // Release previous frame if exists
        if (_currentFrame) {
            esp_camera_fb_return(_currentFrame);
        }
        
        _currentFrame = esp_camera_fb_get();
        if (!_currentFrame) {
            Serial.println("[CAM] Frame capture failed");
            return frame;
        }
        
        frame.data   = _currentFrame->buf;
        frame.len    = _currentFrame->len;
        frame.width  = _currentFrame->width;
        frame.height = _currentFrame->height;
        frame.format = _currentFrame->format;
        
        return frame;
    }
    
    /**
     * Release the current frame buffer back to the driver
     */
    void releaseFrame() {
        if (_currentFrame) {
            esp_camera_fb_return(_currentFrame);
            _currentFrame = nullptr;
        }
    }
    
    /**
     * Set camera resolution
     */
    bool setResolution(CamResolution res) {
        if (!_initialized) return false;
        
        sensor_t* sensor = esp_camera_sensor_get();
        if (!sensor) return false;
        
        _resolution = res;
        return sensor->set_framesize(sensor, (framesize_t)res) == 0;
    }
    
    /**
     * Set pixel format (JPEG, RGB565, etc.)
     */
    bool setFormat(pixformat_t fmt) {
        _format = fmt;
        // Note: Format change requires reinit
        return true;
    }
    
    /**
     * Get current resolution width
     */
    int getWidth() const {
        switch (_resolution) {
            case CAM_RES_QQVGA: return 160;
            case CAM_RES_QVGA:  return 320;
            case CAM_RES_VGA:   return 640;
            case CAM_RES_SVGA:  return 800;
            case CAM_RES_XGA:   return 1024;
            case CAM_RES_HD:    return 1280;
            case CAM_RES_SXGA:  return 1280;
            case CAM_RES_UXGA:  return 1600;
            default:            return 640;
        }
    }
    
    /**
     * Get current resolution height
     */
    int getHeight() const {
        switch (_resolution) {
            case CAM_RES_QQVGA: return 120;
            case CAM_RES_QVGA:  return 240;
            case CAM_RES_VGA:   return 480;
            case CAM_RES_SVGA:  return 600;
            case CAM_RES_XGA:   return 768;
            case CAM_RES_HD:    return 720;
            case CAM_RES_SXGA:  return 1024;
            case CAM_RES_UXGA:  return 1200;
            default:            return 480;
        }
    }
    
    bool isInitialized() const { return _initialized; }
};

// Global camera instance
inline CameraDriver camera;
