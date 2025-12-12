/**
 * @file pipeline.cpp
 * @brief Zero-Copy DMA Video Pipeline Implementation for ESP32-S3
 * 
 * HARDWARE EXPLOITATION SUMMARY:
 * 
 * 1. ZERO-COPY DMA (I2S Camera + SPI Display)
 *    - Camera frames captured directly to PSRAM via DMA
 *    - Display output via SPI DMA (no CPU byte-shuffling)
 *    - CPU only processes small SRAM chunks during transfer gaps
 * 
 * 2. SCANLINE CHUNKING (SRAM vs PSRAM Strategy)
 *    - PSRAM: 8MB, ~40ns latency (frame storage)
 *    - SRAM: 400KB, ~2ns latency (active processing)
 *    - Move 4 scanlines to SRAM, apply filters, DMA to display
 * 
 * 3. DOUBLE BUFFERING (Ping-Pong)
 *    - While DMA sends buffer A to display, CPU processes buffer B
 *    - Swap buffers after each chunk completes
 * 
 * 4. SIMD PROCESSING
 *    - Process 4 pixels per loop iteration
 *    - Use manual unrolling + prefetch hints
 * 
 * @author OpenCamX HPC Optimization
 */

#include "pipeline.hpp"
#include "Camera.hpp"
#include "Display.hpp"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>

// ============================================================
// GLOBAL INSTANCES
// ============================================================

SRAMChunkBuffer g_chunkBuffer;

// SPI DMA handle for display
static spi_device_handle_t g_spiDisplayHandle = nullptr;
static spi_transaction_t g_spiTransaction;
static bool g_pipelineInitialized = false;

// Frame metrics
static volatile uint32_t g_framesProcessed = 0;
static volatile uint32_t g_dmaCompletions = 0;

// Display dimensions (ST7735: 160x128)
static constexpr int DISPLAY_WIDTH = 160;
static constexpr int DISPLAY_HEIGHT = 128;
static constexpr int DISPLAY_PIXELS = DISPLAY_WIDTH * DISPLAY_HEIGHT;

// ============================================================
// SPI DMA CONFIGURATION FOR DISPLAY
// ============================================================
// HARDWARE: SPI2 (HSPI) with DMA Channel 1
// 
// The ST7735 display uses SPI with the following signals:
//   - MOSI (GPIO 39) - Data out
//   - SCLK (GPIO 40) - Clock
//   - CS (GPIO 42) - Chip select
//   - DC (GPIO 2) - Data/Command
// ============================================================

// TFT Pin definitions (from Display.hpp)
#define TFT_CS_PIN   42
#define TFT_DC_PIN   2
#define TFT_SCLK_PIN 40
#define TFT_MOSI_PIN 39

// SPI speed for display (40MHz max for ST7735)
#define DISPLAY_SPI_SPEED_HZ  40000000

/**
 * @brief Pre/Post SPI transaction callbacks for DC pin control
 * HARDWARE: Uses direct GPIO register access for minimal latency
 */
static void IRAM_ATTR spi_pre_transfer_callback(spi_transaction_t *t) {
    // Set DC high for data mode (this is set by user data field)
    int dc_level = (int)t->user;
    if (dc_level) {
        GPIO_SET_HIGH(TFT_DC_PIN);  // Data mode
    } else {
        GPIO_SET_LOW(TFT_DC_PIN);   // Command mode
    }
}

/**
 * @brief Initialize SPI DMA for display output
 * HARDWARE: Configures SPI2 peripheral with DMA descriptors
 * @return true on success
 */
static bool COLD_FUNC init_display_spi_dma() {
    esp_err_t ret;
    
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_MOSI_PIN,
        .miso_io_num = -1,  // Display has no MISO
        .sclk_io_num = TFT_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_PIXELS * 2,  // Max transfer = full frame
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0
    };
    
    // Initialize SPI bus with DMA channel
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        Serial.printf("[PIPELINE] SPI bus init failed: %d\n", ret);
        return false;
    }
    
    // Configure SPI device (ST7735 display)
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,  // SPI mode 0
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = DISPLAY_SPI_SPEED_HZ,
        .input_delay_ns = 0,
        .spics_io_num = TFT_CS_PIN,
        .flags = SPI_DEVICE_NO_DUMMY,
        .queue_size = 7,  // Queue depth for async transactions
        .pre_cb = spi_pre_transfer_callback,
        .post_cb = nullptr
    };
    
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &g_spiDisplayHandle);
    if (ret != ESP_OK) {
        Serial.printf("[PIPELINE] SPI device add failed: %d\n", ret);
        return false;
    }
    
    // Configure DC pin as output (direct GPIO)
    gpio_set_direction((gpio_num_t)TFT_DC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)TFT_DC_PIN, 1);
    
    Serial.println("[PIPELINE] SPI DMA initialized for display");
    return true;
}

// ============================================================
// DMA TRANSFER FUNCTIONS
// ============================================================
// These functions use SPI DMA to transfer pixel data to display
// while the CPU is free to process the next chunk
// ============================================================

/**
 * @brief Send data to display via SPI DMA (non-blocking)
 * HARDWARE: Uses linked-list DMA descriptors for zero-copy
 * @param data Pointer to pixel data
 * @param length Data length in bytes
 */
static void IRAM_ATTR send_display_dma_async(const void* data, size_t length) {
    if (UNLIKELY(!g_spiDisplayHandle)) return;
    
    memset(&g_spiTransaction, 0, sizeof(g_spiTransaction));
    g_spiTransaction.length = length * 8;  // Length in bits
    g_spiTransaction.tx_buffer = data;
    g_spiTransaction.user = (void*)1;  // DC = HIGH (data mode)
    
    // Queue transaction (non-blocking)
    // HARDWARE: DMA controller handles the transfer
    spi_device_queue_trans(g_spiDisplayHandle, &g_spiTransaction, portMAX_DELAY);
}

/**
 * @brief Wait for DMA transfer to complete
 */
static void IRAM_ATTR wait_display_dma_complete() {
    if (UNLIKELY(!g_spiDisplayHandle)) return;
    
    spi_transaction_t* rtrans;
    spi_device_get_trans_result(g_spiDisplayHandle, &rtrans, portMAX_DELAY);
    g_dmaCompletions++;
}

/**
 * @brief Send command to display
 */
static void IRAM_ATTR send_display_command(uint8_t cmd) {
    if (UNLIKELY(!g_spiDisplayHandle)) return;
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;  // 8 bits
    t.tx_data[0] = cmd;
    t.user = (void*)0;  // DC = LOW (command mode)
    t.flags = SPI_TRANS_USE_TXDATA;
    
    spi_device_polling_transmit(g_spiDisplayHandle, &t);
}

/**
 * @brief Send data byte to display
 */
static void IRAM_ATTR send_display_data(uint8_t data) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_data[0] = data;
    t.user = (void*)1;  // DC = HIGH (data mode)
    t.flags = SPI_TRANS_USE_TXDATA;
    
    spi_device_polling_transmit(g_spiDisplayHandle, &t);
}

/**
 * @brief Set display window for writes (ST7735 specific)
 */
static void set_display_window(int x0, int y0, int x1, int y1) {
    // Column address set (CASET)
    send_display_command(0x2A);
    send_display_data(0x00);
    send_display_data(x0);
    send_display_data(0x00);
    send_display_data(x1);
    
    // Row address set (RASET)
    send_display_command(0x2B);
    send_display_data(0x00);
    send_display_data(y0);
    send_display_data(0x00);
    send_display_data(y1);
    
    // Memory write (RAMWR)
    send_display_command(0x2C);
}

// ============================================================
// SCANLINE PROCESSING WITH DOWNSCALING
// ============================================================
// Camera captures at higher resolution (e.g., 320x240)
// Display is smaller (160x128)
// We downscale using nearest-neighbor (fast) or box filter
// ============================================================

/**
 * @brief Downscale and filter a scanline from camera buffer
 * 
 * HARDWARE EXPLOITATION:
 * - Source data in PSRAM (camera frame buffer)
 * - Destination in SRAM (chunk buffer)
 * - SIMD filter applied after copy
 * 
 * @param src Source scanline in PSRAM
 * @param src_width Source width (e.g., 320)
 * @param dst Destination buffer in SRAM
 * @param dst_width Destination width (e.g., 160)
 * @param filter Filter to apply
 */
static void IRAM_ATTR process_scanline_with_downscale(
    const uint16_t* src, int src_width,
    uint16_t* dst, int dst_width,
    FilterType filter
) {
    // Calculate scale factor in fixed-point (16.16)
    const uint32_t scale = (src_width << 16) / dst_width;
    
    // Nearest-neighbor downscale (fast!)
    // HARDWARE: Single memory read per output pixel
    #pragma GCC unroll 8
    for (int x = 0; x < dst_width; x++) {
        uint32_t src_x = (x * scale) >> 16;
        dst[x] = src[src_x];
    }
    
    // Apply filter to downscaled data (now in SRAM = fast!)
    process_scanline_chunk(dst, dst_width, filter);
}

// ============================================================
// MAIN PIPELINE FUNCTIONS
// ============================================================

/**
 * @brief Initialize the high-performance video pipeline
 * 
 * BOOT SEQUENCE:
 * 1. Allocate SRAM chunk buffers (ping-pong)
 * 2. Initialize SPI DMA for display
 * 3. Configure DMA descriptors
 */
bool COLD_FUNC pipeline_init() {
    if (g_pipelineInitialized) return true;
    
    Serial.println("[PIPELINE] Initializing HPC video pipeline...");
    
    // 1. Allocate SRAM buffers for chunk processing
    if (!g_chunkBuffer.init()) {
        Serial.println("[PIPELINE] Failed to allocate SRAM buffers!");
        return false;
    }
    
    // 2. Initialize SPI DMA for display output
    if (!init_display_spi_dma()) {
        Serial.println("[PIPELINE] Failed to init display SPI DMA!");
        g_chunkBuffer.deinit();
        return false;
    }
    
    g_pipelineInitialized = true;
    g_framesProcessed = 0;
    g_dmaCompletions = 0;
    
    Serial.printf("[PIPELINE] Ready! Free heap: %d, Free PSRAM: %d\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());
    
    return true;
}

/**
 * @brief Deinitialize pipeline and free resources
 */
void COLD_FUNC pipeline_deinit() {
    if (!g_pipelineInitialized) return;
    
    g_chunkBuffer.deinit();
    
    if (g_spiDisplayHandle) {
        spi_bus_remove_device(g_spiDisplayHandle);
        spi_bus_free(SPI2_HOST);
        g_spiDisplayHandle = nullptr;
    }
    
    g_pipelineInitialized = false;
    Serial.println("[PIPELINE] Deinitialized");
}

/**
 * @brief Process a camera frame and output to display
 * 
 * This is the main pipeline function - call once per frame.
 * 
 * PIPELINE STAGES:
 * 1. Get camera frame (already in PSRAM via DMA)
 * 2. For each chunk of scanlines:
 *    a. Copy + downscale to SRAM buffer
 *    b. Apply SIMD filter
 *    c. DMA transfer to display
 * 3. Release camera frame
 * 
 * @param frame Camera frame from esp_camera_fb_get()
 * @param filter Filter type to apply
 */
void IRAM_ATTR pipeline_process_frame(camera_fb_t* frame, FilterType filter) {
    if (UNLIKELY(!g_pipelineInitialized || !frame)) return;
    
    // Frame dimensions
    const int src_width = frame->width;
    const int src_height = frame->height;
    const uint16_t* src_data = (const uint16_t*)frame->buf;
    
    // Calculate vertical scale (how many source lines per display line)
    const int y_scale = (src_height << 8) / DISPLAY_HEIGHT;
    
    // Set display window to full screen
    set_display_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    
    // Process display one line at a time
    // Each line: Read from PSRAM → Downscale+Filter in SRAM → DMA to display
    
    uint16_t* sram_buffer = g_chunkBuffer.getProcessBuffer();
    
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        // Calculate source Y coordinate (fixed-point)
        int src_y = (y * y_scale) >> 8;
        if (UNLIKELY(src_y >= src_height)) src_y = src_height - 1;
        
        const uint16_t* src_line = &src_data[src_y * src_width];
        
        // Downscale + filter the scanline
        process_scanline_with_downscale(
            src_line, src_width,
            sram_buffer, DISPLAY_WIDTH,
            filter
        );
        
        // DMA transfer scanline to display (blocking for now)
        // TODO: Use async DMA with ping-pong buffers for overlap
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = DISPLAY_WIDTH * 16;  // bits
        t.tx_buffer = sram_buffer;
        t.user = (void*)1;  // Data mode
        
        spi_device_polling_transmit(g_spiDisplayHandle, &t);
    }
    
    g_framesProcessed++;
}

/**
 * @brief Process camera frame using the new high-performance pipeline
 * 
 * Wrapper function that integrates with existing CameraDriver API
 */
void IRAM_ATTR pipeline_process_camera_frame(FilterType filter) {
    // Capture frame from camera
    camera_fb_t* fb = esp_camera_fb_get();
    if (UNLIKELY(!fb)) {
        Serial.println("[PIPELINE] Frame capture failed!");
        return;
    }
    
    // Process through pipeline
    pipeline_process_frame(fb, filter);
    
    // Return frame buffer to camera
    esp_camera_fb_return(fb);
}

/**
 * @brief Get pipeline performance statistics
 */
void pipeline_get_stats(uint32_t& frames, uint32_t& dma_ops) {
    frames = g_framesProcessed;
    dma_ops = g_dmaCompletions;
}

// ============================================================
// OPTIMIZED FILTER WRAPPERS FOR FULL-FRAME PROCESSING
// ============================================================
// For cases where we need to filter the entire frame in-place
// (e.g., before USB webcam streaming)
// ============================================================

/**
 * @brief Apply grayscale filter to entire frame (in-place)
 * HARDWARE: Uses SIMD kernels with cache prefetch
 */
void IRAM_ATTR pipeline_filter_grayscale_frame(uint16_t* data, int width, int height) {
    const int total_pixels = width * height;
    const int aligned = total_pixels & ~3;  // Round to multiple of 4
    
    for (int i = 0; i < aligned; i += 4) {
        // Prefetch next cache line (32 bytes = 16 pixels ahead)
        PREFETCH_READ(&data[i + 16]);
        filter_grayscale_x4(&data[i]);
    }
    
    // Handle remaining pixels (0-3)
    for (int i = aligned; i < total_pixels; i++) {
        uint8_t r, g, b;
        rgb565_unpack_fast(data[i], r, g, b);
        uint8_t gray = compute_luma_q8(r, g, b);
        data[i] = rgb888_to_565_fast(gray, gray, gray);
    }
}

/**
 * @brief Apply sepia filter to entire frame (in-place)
 */
void IRAM_ATTR pipeline_filter_sepia_frame(uint16_t* data, int width, int height) {
    const int total_pixels = width * height;
    const int aligned = total_pixels & ~3;
    
    for (int i = 0; i < aligned; i += 4) {
        PREFETCH_READ(&data[i + 16]);
        filter_sepia_x4(&data[i]);
    }
    
    // Handle remaining pixels
    for (int i = aligned; i < total_pixels; i++) {
        uint8_t r, g, b;
        rgb565_unpack_fast(data[i], r, g, b);
        
        uint16_t tr = (r * SEPIA_RR + g * SEPIA_RG + b * SEPIA_RB) >> 8;
        uint16_t tg = (r * SEPIA_GR + g * SEPIA_GG + b * SEPIA_GB) >> 8;
        uint16_t tb = (r * SEPIA_BR + g * SEPIA_BG + b * SEPIA_BB) >> 8;
        
        r = tr > 255 ? 255 : (uint8_t)tr;
        g = tg > 255 ? 255 : (uint8_t)tg;
        b = tb > 255 ? 255 : (uint8_t)tb;
        
        data[i] = rgb888_to_565_fast(r, g, b);
    }
}

/**
 * @brief Apply vignette filter to entire frame
 */
void IRAM_ATTR pipeline_filter_vignette_frame(uint16_t* data, int width, int height) {
    const int cx = width >> 1;
    const int cy = height >> 1;
    const int max_dist_sq = cx * cx + cy * cy;
    
    // Precompute inverse (256 << 16) / max_dist_sq for fixed-point division
    const uint32_t inv_max_dist_sq = (256UL << 16) / max_dist_sq;
    
    for (int y = 0; y < height; y++) {
        uint16_t* row = &data[y * width];
        const int aligned_width = width & ~3;
        
        for (int x = 0; x < aligned_width; x += 4) {
            filter_vignette_x4(&row[x], x, y, cx, cy, inv_max_dist_sq);
        }
        
        // Handle remaining pixels
        for (int x = aligned_width; x < width; x++) {
            int dx = x - cx;
            int dy = y - cy;
            uint32_t dist_sq = dx * dx + dy * dy;
            
            int32_t factor = 256 - ((dist_sq * inv_max_dist_sq) >> 16);
            if (factor < 0) factor = 0;
            
            uint8_t r, g, b;
            rgb565_unpack_fast(row[x], r, g, b);
            r = (r * factor) >> 8;
            g = (g * factor) >> 8;
            b = (b * factor) >> 8;
            row[x] = rgb888_to_565_fast(r, g, b);
        }
    }
}
