#pragma once
/**
 * @file FilterChain.hpp
 * @brief Image Filter Pipeline for OpenCamX (ESP32 Optimized)
 * 
 * Chainable filter architecture for real-time image processing.
 * Works on RGB565 frame buffers from the camera.
 * 
 * MEMORY OPTIMIZED: No std::vector, no dynamic allocation,
 * integer-only math for 400KB ESP32 constraint.
 */

#include <Arduino.h>

// Maximum filters in chain (fixed allocation)
#define MAX_FILTERS 8

// ============================================================
// Viewfinder Downsampling Configuration
// ============================================================
// RATIONALE: Processing full-res (1600x1200) UXGA on ESP32 yields <2 FPS
// Viewfinder uses downscaled resolution (320x240) for smooth 15-30 FPS
// Full resolution is only used for photo/video capture
// ============================================================
#define VIEWFINDER_WIDTH   320
#define VIEWFINDER_HEIGHT  240
#define VIEWFINDER_PIXELS  (VIEWFINDER_WIDTH * VIEWFINDER_HEIGHT)

// Processing mode enum
enum FilterMode {
    FILTER_MODE_VIEWFINDER,  // Fast, downscaled for display (320x240)
    FILTER_MODE_CAPTURE      // Full resolution for saving (up to 1600x1200)
};

// ============================================================
// RGB565 Pixel Helpers
// ============================================================
inline void rgb565_to_rgb888(uint16_t pixel, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = ((pixel >> 11) & 0x1F) << 3;
    g = ((pixel >> 5) & 0x3F) << 2;
    b = (pixel & 0x1F) << 3;
}

inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// ============================================================
// Filter Base Class
// ============================================================
class ImageFilter {
public:
    const char* name;
    bool enabled = true;
    
    ImageFilter(const char* filterName) : name(filterName) {}
    virtual ~ImageFilter() = default;
    
    /**
     * Process a frame in-place
     * @param data RGB565 pixel data
     * @param width Frame width
     * @param height Frame height
     */
    virtual void process(uint16_t* data, int width, int height) = 0;
};

// ============================================================
// Grayscale Filter
// ============================================================
class GrayscaleFilter : public ImageFilter {
public:
    GrayscaleFilter() : ImageFilter("Grayscale") {}
    
    void process(uint16_t* data, int width, int height) override {
        for (int i = 0; i < width * height; i++) {
            uint8_t r, g, b;
            rgb565_to_rgb888(data[i], r, g, b);
            
            // Luminance formula
            uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
            data[i] = rgb888_to_rgb565(gray, gray, gray);
        }
    }
};

// ============================================================
// Sepia Filter
// ============================================================
class SepiaFilter : public ImageFilter {
public:
    SepiaFilter() : ImageFilter("Sepia") {}
    
    void process(uint16_t* data, int width, int height) override {
        for (int i = 0; i < width * height; i++) {
            uint8_t r, g, b;
            rgb565_to_rgb888(data[i], r, g, b);
            
            // Sepia transformation using integer math (coefficients * 256)
            // Original: 0.393*r + 0.769*g + 0.189*b → 101*r + 197*g + 48*b >> 8
            int tr = (r * 101 + g * 197 + b * 48) >> 8;
            int tg = (r * 89 + g * 176 + b * 43) >> 8;
            int tb = (r * 70 + g * 137 + b * 34) >> 8;
            
            data[i] = rgb888_to_rgb565(
                min(255, tr),
                min(255, tg),
                min(255, tb)
            );
        }
    }
};

// ============================================================
// Vignette Filter
// ============================================================
class VignetteFilter : public ImageFilter {
public:
    uint8_t m_strength_q8 = 128;  // Q8 fixed-point: 128 = 0.5, 256 = 1.0
    
    VignetteFilter() : ImageFilter("Vignette") {}
    
    // Getter/setter for compatibility with existing code expecting float
    void setStrength(float s) { m_strength_q8 = (uint8_t)(s * 256); }
    float getStrength() const { return m_strength_q8 / 256.0f; }
    
    void process(uint16_t* data, int width, int height) override {
        // Fixed-point vignette: avoid sqrt() by using squared distance
        // Distance comparison is monotonic, so squared works for darkening
        const int cx = width >> 1;
        const int cy = height >> 1;
        const int maxDistSq = cx * cx + cy * cy;  // Pre-compute max distance squared
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;
                
                int dx = x - cx;
                int dy = y - cy;
                int distSq = dx * dx + dy * dy;
                
                // Linear interpolation in Q8: factor = 256 - (distSq * strength / maxDistSq)
                // Clamp to avoid overflow: (distSq * m_strength_q8) could be large
                int factor = 256 - ((distSq * m_strength_q8) / maxDistSq);
                if (factor < 0) factor = 0;
                
                uint8_t r, g, b;
                rgb565_to_rgb888(data[idx], r, g, b);
                
                // Apply factor (Q8 division by 256 via >>8)
                data[idx] = rgb888_to_rgb565(
                    (r * factor) >> 8,
                    (g * factor) >> 8,
                    (b * factor) >> 8
                );
            }
        }
    }
    
    // Legacy float member for backward compatibility (deprecated)
    float strength = 0.5f;  // WARNING: Use m_strength_q8 for performance
};

// ============================================================
// Film Grain Filter
// ============================================================
class GrainFilter : public ImageFilter {
public:
    uint8_t intensity = 30;  // Noise intensity (0-255)
    
    GrainFilter() : ImageFilter("Grain") {}
    
    void process(uint16_t* data, int width, int height) override {
        for (int i = 0; i < width * height; i++) {
            uint8_t r, g, b;
            rgb565_to_rgb888(data[i], r, g, b);
            
            // Random noise
            int noise = (random(256) - 128) * intensity / 255;
            
            r = constrain(r + noise, 0, 255);
            g = constrain(g + noise, 0, 255);
            b = constrain(b + noise, 0, 255);
            
            data[i] = rgb888_to_rgb565(r, g, b);
        }
    }
};

// ============================================================
// Edge Detection Filter (Sobel) - ESP32 OPTIMIZED
// Uses line-by-line processing to avoid large buffer allocation
// ============================================================
class EdgeDetectFilter : public ImageFilter {
public:
    uint8_t threshold = 50;
    
    EdgeDetectFilter() : ImageFilter("EdgeDetect") {}
    
    void process(uint16_t* data, int width, int height) override {
        // Process in-place using only 3 line buffers (saves huge memory)
        // Only allocate 3 lines worth of grayscale: 3 * width bytes
        static uint8_t lineBuffer[3][320];  // Max width 320 for QVGA
        
        if (width > 320) return;  // Skip if too large
        
        // Fill initial line buffers
        for (int row = 0; row < 3 && row < height; row++) {
            for (int x = 0; x < width; x++) {
                uint8_t r, g, b;
                rgb565_to_rgb888(data[row * width + x], r, g, b);
                lineBuffer[row][x] = (r * 77 + g * 150 + b * 29) >> 8;
            }
        }
        
        // Process row by row
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                int bufY = y % 3;
                int bufYm1 = (y - 1) % 3;
                int bufYp1 = (y + 1) % 3;
                
                // Sobel operator using line buffers
                int sumX = -lineBuffer[bufYm1][x-1] + lineBuffer[bufYm1][x+1]
                          -2*lineBuffer[bufY][x-1] + 2*lineBuffer[bufY][x+1]
                          -lineBuffer[bufYp1][x-1] + lineBuffer[bufYp1][x+1];
                          
                int sumY = -lineBuffer[bufYm1][x-1] - 2*lineBuffer[bufYm1][x] - lineBuffer[bufYm1][x+1]
                          +lineBuffer[bufYp1][x-1] + 2*lineBuffer[bufYp1][x] + lineBuffer[bufYp1][x+1];
                
                // Fast magnitude approximation (no sqrt)
                int mag = (abs(sumX) + abs(sumY)) >> 1;
                uint8_t edge = mag > threshold ? 255 : 0;
                
                data[y * width + x] = rgb888_to_rgb565(edge, edge, edge);
            }
            
            // Load next line into buffer (circular)
            if (y + 2 < height) {
                int nextRow = (y + 2) % 3;
                for (int x = 0; x < width; x++) {
                    uint8_t r, g, b;
                    rgb565_to_rgb888(data[(y + 2) * width + x], r, g, b);
                    lineBuffer[nextRow][x] = (r * 77 + g * 150 + b * 29) >> 8;
                }
            }
        }
    }
};

// ============================================================
// Filter Chain Manager - ESP32 OPTIMIZED (No dynamic allocation)
// ============================================================
class FilterChain {
private:
    ImageFilter* _filters[MAX_FILTERS];
    int _count = 0;

public:
    FilterChain() {
        for (int i = 0; i < MAX_FILTERS; i++) _filters[i] = nullptr;
    }
    
    /**
     * Add a filter to the chain
     */
    bool addFilter(ImageFilter* filter) {
        if (_count >= MAX_FILTERS) return false;
        _filters[_count++] = filter;
        Serial.printf("[FILTER] Added: %s\n", filter->name);
        return true;
    }
    
    /**
     * Remove all filters
     */
    void clear() {
        _count = 0;
        for (int i = 0; i < MAX_FILTERS; i++) _filters[i] = nullptr;
    }
    
    /**
     * Process frame through all enabled filters
     */
    void process(uint16_t* data, int width, int height) {
        for (int i = 0; i < _count; i++) {
            if (_filters[i] && _filters[i]->enabled) {
                _filters[i]->process(data, width, height);
            }
        }
    }
    
    /**
     * Toggle a filter by name
     */
    bool toggle(const char* name) {
        for (int i = 0; i < _count; i++) {
            if (_filters[i] && strcmp(_filters[i]->name, name) == 0) {
                _filters[i]->enabled = !_filters[i]->enabled;
                Serial.printf("[FILTER] %s: %s\n", name, _filters[i]->enabled ? "ON" : "OFF");
                return true;
            }
        }
        return false;
    }
    
    /**
     * Enable/disable a filter by index
     */
    void setEnabled(int index, bool enabled) {
        if (index >= 0 && index < _count && _filters[index]) {
            _filters[index]->enabled = enabled;
        }
    }
    
    int count() const { return _count; }
    ImageFilter* get(int index) { return (index < _count) ? _filters[index] : nullptr; }
    
    /**
     * Process frame with mode awareness (viewfinder vs capture)
     * Viewfinder mode: Fast processing on downscaled buffer
     * Capture mode: Full quality processing on original resolution
     */
    void processWithMode(uint16_t* data, int width, int height, FilterMode mode) {
        if (mode == FILTER_MODE_VIEWFINDER && (width > VIEWFINDER_WIDTH || height > VIEWFINDER_HEIGHT)) {
            // For viewfinder, process at display resolution for speed
            Serial.printf("[FILTER] Skipping full-res filters for viewfinder (%dx%d)\n", width, height);
            return;
        }
        // Process normally
        process(data, width, height);
    }
};

// ============================================================
// Fast Downscaling Utility (Box Filter)
// ============================================================
// Downscales a frame using nearest-neighbor sampling
// Used for viewfinder display to maintain smooth FPS
// ============================================================
inline void downscaleFrame(
    const uint16_t* src, int srcWidth, int srcHeight,
    uint16_t* dst, int dstWidth, int dstHeight
) {
    // Fixed-point scaling factors (16.16 format)
    const int scaleX = (srcWidth << 16) / dstWidth;
    const int scaleY = (srcHeight << 16) / dstHeight;
    
    for (int y = 0; y < dstHeight; y++) {
        int srcY = (y * scaleY) >> 16;
        for (int x = 0; x < dstWidth; x++) {
            int srcX = (x * scaleX) >> 16;
            dst[y * dstWidth + x] = src[srcY * srcWidth + srcX];
        }
    }
}

// ============================================================
// VINTAGE MODE FILTER (CPU Optimized - Integer Math)
// Warm tones, slight desaturation, faded blacks
// ============================================================
class VintageFilter : public ImageFilter {
public:
    uint8_t fadeAmount = 20;    // How much to fade blacks (0-50)
    uint8_t warmth = 30;        // Orange/yellow shift (0-100)
    
    VintageFilter() : ImageFilter("Vintage") {}
    
    void process(uint16_t* data, int width, int height) override {
        const int total = width * height;
        for (int i = 0; i < total; i++) {
            uint8_t r, g, b;
            rgb565_to_rgb888(data[i], r, g, b);
            
            // 1. Slight desaturation using integer luminance
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = (r * 180 + lum * 76) >> 8;  // 70% original + 30% gray
            g = (g * 180 + lum * 76) >> 8;
            b = (b * 180 + lum * 76) >> 8;
            
            // 2. Add warmth (shift towards orange/yellow)
            int rWarm = r + warmth;
            int gWarm = g + (warmth >> 1);  // Half warmth to green
            
            // 3. Fade blacks (lift shadows)
            r = min(255, max((int)fadeAmount, rWarm));
            g = min(255, max((int)fadeAmount, gWarm));
            b = min(255, max((int)fadeAmount, (int)b));
            
            data[i] = rgb888_to_rgb565(r, g, b);
        }
    }
};

// ============================================================
// COOL MODE FILTER (CPU Optimized - Integer Math)
// Blue/cyan tint, increased contrast, crisp look
// ============================================================
class CoolFilter : public ImageFilter {
public:
    uint8_t coolness = 25;      // Blue shift amount (0-50)
    uint8_t contrast = 20;      // Contrast boost (0-50)
    
    CoolFilter() : ImageFilter("Cool") {}
    
    void process(uint16_t* data, int width, int height) override {
        const int total = width * height;
        for (int i = 0; i < total; i++) {
            uint8_t r, g, b;
            rgb565_to_rgb888(data[i], r, g, b);
            
            // 1. Apply contrast using integer math
            // Formula: ((pixel - 128) * (256 + contrast)) >> 8 + 128
            int rC = ((((int)r - 128) * (256 + contrast * 2)) >> 8) + 128;
            int gC = ((((int)g - 128) * (256 + contrast * 2)) >> 8) + 128;
            int bC = ((((int)b - 128) * (256 + contrast * 2)) >> 8) + 128;
            
            // 2. Add cool blue/cyan tint
            rC = rC - (coolness >> 1);  // Reduce red
            gC = gC + (coolness >> 2);  // Slight cyan boost
            bC = bC + coolness;          // Boost blue
            
            // Clamp values
            r = constrain(rC, 0, 255);
            g = constrain(gC, 0, 255);
            b = constrain(bC, 0, 255);
            
            data[i] = rgb888_to_rgb565(r, g, b);
        }
    }
};

// ============================================================
// VIBRANT MODE FILTER (CPU Optimized - Integer Math)
// High saturation, punchy colors, enhanced clarity
// ============================================================
class VibrantFilter : public ImageFilter {
public:
    uint8_t saturationBoost = 40;   // Saturation increase (0-100)
    uint8_t clarityBoost = 15;       // Micro-contrast (0-50)
    
    VibrantFilter() : ImageFilter("Vibrant") {}
    
    void process(uint16_t* data, int width, int height) override {
        const int total = width * height;
        for (int i = 0; i < total; i++) {
            uint8_t r, g, b;
            rgb565_to_rgb888(data[i], r, g, b);
            
            // 1. Calculate luminance (integer)
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            
            // 2. Boost saturation by moving away from gray
            // Formula: pixel = lum + (pixel - lum) * boost_factor
            int boostFactor = 256 + saturationBoost * 2;  // 1.0 + boost
            
            int rS = lum + ((((int)r - lum) * boostFactor) >> 8);
            int gS = lum + ((((int)g - lum) * boostFactor) >> 8);
            int bS = lum + ((((int)b - lum) * boostFactor) >> 8);
            
            // 3. Add micro-contrast (small contrast boost)
            rS = ((rS - 128) * (256 + clarityBoost)) / 256 + 128;
            gS = ((gS - 128) * (256 + clarityBoost)) / 256 + 128;
            bS = ((bS - 128) * (256 + clarityBoost)) / 256 + 128;
            
            // Clamp
            r = constrain(rS, 0, 255);
            g = constrain(gS, 0, 255);
            b = constrain(bS, 0, 255);
            
            data[i] = rgb888_to_rgb565(r, g, b);
        }
    }
};

// ============================================================
// FAST BLUR FILTER (CPU Optimized - Box Blur)
// 3x3 box blur using integer averaging
// ============================================================
class BlurFilter : public ImageFilter {
public:
    BlurFilter() : ImageFilter("Blur") {}
    
    void process(uint16_t* data, int width, int height) override {
        // Simple box blur - process every other pixel for speed
        for (int y = 1; y < height - 1; y += 2) {
            for (int x = 1; x < width - 1; x += 2) {
                int rSum = 0, gSum = 0, bSum = 0;
                
                // 3x3 neighborhood
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        uint8_t r, g, b;
                        rgb565_to_rgb888(data[(y + ky) * width + (x + kx)], r, g, b);
                        rSum += r;
                        gSum += g;
                        bSum += b;
                    }
                }
                
                // Average (divide by 9 using shift approximation: /9 ≈ *7/64)
                uint8_t rAvg = (rSum * 7) >> 6;
                uint8_t gAvg = (gSum * 7) >> 6;
                uint8_t bAvg = (bSum * 7) >> 6;
                
                data[y * width + x] = rgb888_to_rgb565(rAvg, gAvg, bAvg);
            }
        }
    }
};

// ============================================================
// SHARPEN FILTER (CPU Optimized)
// ============================================================
class SharpenFilter : public ImageFilter {
public:
    uint8_t strength = 30;  // Sharpening strength (0-100)
    
    SharpenFilter() : ImageFilter("Sharpen") {}
    
    void process(uint16_t* data, int width, int height) override {
        // Process inner pixels only
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                uint8_t r, g, b, rN, gN, bN;
                int idx = y * width + x;
                
                rgb565_to_rgb888(data[idx], r, g, b);
                
                // Get neighbors average
                int rSum = 0, gSum = 0, bSum = 0;
                rgb565_to_rgb888(data[idx - 1], rN, gN, bN); rSum += rN; gSum += gN; bSum += bN;
                rgb565_to_rgb888(data[idx + 1], rN, gN, bN); rSum += rN; gSum += gN; bSum += bN;
                rgb565_to_rgb888(data[idx - width], rN, gN, bN); rSum += rN; gSum += gN; bSum += bN;
                rgb565_to_rgb888(data[idx + width], rN, gN, bN); rSum += rN; gSum += gN; bSum += bN;
                
                // Sharpen: pixel + strength * (pixel - neighbor_avg)
                int rSharp = r + ((strength * (r * 4 - rSum)) >> 8);
                int gSharp = g + ((strength * (g * 4 - gSum)) >> 8);
                int bSharp = b + ((strength * (b * 4 - bSum)) >> 8);
                
                data[idx] = rgb888_to_rgb565(
                    constrain(rSharp, 0, 255),
                    constrain(gSharp, 0, 255),
                    constrain(bSharp, 0, 255)
                );
            }
        }
    }
};

// ============================================================
// Global Filter Instances (All CPU-Optimized)
// ============================================================
inline GrayscaleFilter grayscaleFilter;
inline SepiaFilter sepiaFilter;
inline VignetteFilter vignetteFilter;
inline GrainFilter grainFilter;
inline EdgeDetectFilter edgeDetectFilter;
inline VintageFilter vintageFilter;
inline CoolFilter coolFilter;
inline VibrantFilter vibrantFilter;
inline BlurFilter blurFilter;
inline SharpenFilter sharpenFilter;
inline FilterChain filterChain;

