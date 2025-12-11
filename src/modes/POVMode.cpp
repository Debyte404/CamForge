/**
 * @file POVMode.cpp
 * @brief POV Camera Recording Mode for OpenCamX
 * 
 * Records video clips to SD card in MJPEG/AVI format.
 * Start/stop via button, includes timestamp overlay.
 */

#include "../core/Game.hpp"
#include "../core/Input.hpp"
#include "../core/Camera.hpp"
#include "../core/ModeBase.hpp"
#include "../drivers/SDCard.hpp"
#include "../drivers/LED.hpp"
#include <Adafruit_ST7735.h>

extern Adafruit_ST7735 tft;

// ============================================================
// AVI Header Structures (MJPEG format)
// ============================================================

// Simple RIFF AVI header for MJPEG video
struct AVIHeader {
    // RIFF header
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;  // Will be updated on close
    char avi[4] = {'A', 'V', 'I', ' '};
    
    // hdrl LIST
    char hdrl[4] = {'L', 'I', 'S', 'T'};
    uint32_t hdrlSize = 0;
    char hdr1Type[4] = {'h', 'd', 'r', 'l'};
    
    // avih chunk
    char avih[4] = {'a', 'v', 'i', 'h'};
    uint32_t avihSize = 56;
    uint32_t microSecPerFrame = 33333;  // 30 fps
    uint32_t maxBytesPerSec = 0;
    uint32_t paddingGranularity = 0;
    uint32_t flags = 0x10;  // AVIF_HASINDEX
    uint32_t totalFrames = 0;
    uint32_t initialFrames = 0;
    uint32_t streams = 1;
    uint32_t suggestedBufferSize = 0;
    uint32_t width = 640;
    uint32_t height = 480;
    uint32_t reserved[4] = {0};
};

// ============================================================
// Recording State
// ============================================================
static bool povActive = false;
static bool isRecording = false;
static File recordingFile;
static uint32_t frameCount = 0;
static uint32_t recordingStartTime = 0;
static char g_current_filename[48];  // Fixed buffer to avoid heap fragmentation

// ============================================================
// Helper Functions
// ============================================================

void drawPOVStatus() {
    tft.fillRect(0, 0, tft.width(), 30, ST77XX_BLACK);
    
    if (isRecording) {
        // Red recording indicator
        tft.fillCircle(15, 15, 8, ST77XX_RED);
        tft.setTextColor(ST77XX_RED);
        tft.setTextSize(1);
        tft.setCursor(30, 10);
        tft.print("REC");
        
        // Duration
        uint32_t duration = (millis() - recordingStartTime) / 1000;
        tft.setCursor(60, 10);
        tft.printf("%02d:%02d", duration / 60, duration % 60);
        
        // Frame count
        tft.setCursor(100, 10);
        tft.printf("F:%d", frameCount);
    } else {
        tft.setTextColor(ST77XX_GREEN);
        tft.setTextSize(1);
        tft.setCursor(10, 10);
        tft.print("READY");
    }
}

bool startRecording() {
    if (!sdCard.isMounted()) {
        Serial.println("[POV] SD card not mounted!");
        return false;
    }
    
    if (!sdCard.generateFilenameSafe(g_current_filename, sizeof(g_current_filename), "VID", "avi")) {
        Serial.println("[POV] Filename buffer overflow!");
        return false;
    }
    recordingFile = sdCard.openFile(g_current_filename, FILE_WRITE);
    
    if (!recordingFile) {
        Serial.println("[POV] Failed to create file!");
        return false;
    }
    
    // Write AVI header placeholder (will be updated on close)
    AVIHeader header;
    header.width = camera.getWidth();
    header.height = camera.getHeight();
    recordingFile.write((uint8_t*)&header, sizeof(header));
    
    // Write movi LIST start
    recordingFile.write((const uint8_t*)"LIST", 4);
    uint32_t moviSizePlaceholder = 0;
    recordingFile.write((uint8_t*)&moviSizePlaceholder, 4);
    recordingFile.write((const uint8_t*)"movi", 4);
    
    frameCount = 0;
    recordingStartTime = millis();
    isRecording = true;
    
    Serial.printf("[POV] Recording started: %s\n", g_current_filename);
    return true;
}

void stopRecording() {
    if (!isRecording) return;
    
    isRecording = false;
    
    // TODO: Update AVI header with final frame count and file size
    // For MVP, we just close the file
    recordingFile.close();
    
    Serial.printf("[POV] Recording stopped: %d frames saved\n", frameCount);
    
    // Flash LED to indicate save complete
    led.on();
    delay(200);
    led.off();
}

void recordFrame() {
    if (!isRecording) return;
    
    CamFrame frame = camera.captureFrame();
    if (!frame.valid()) return;
    
    // Write JPEG frame as 00dc chunk
    recordingFile.write((const uint8_t*)"00dc", 4);
    uint32_t frameSize = frame.len;
    recordingFile.write((uint8_t*)&frameSize, 4);
    recordingFile.write(frame.data, frame.len);
    
    // Pad to word boundary
    if (frame.len % 2) {
        uint8_t pad = 0;
        recordingFile.write(&pad, 1);
    }
    
    frameCount++;
    camera.releaseFrame();
}

// ============================================================
// POV Mode Functions
// ============================================================

void povInit() {
    Serial.println("[POV] Initializing...");
    
    // Initialize camera
    if (!camera.isInitialized()) {
        camera.init();
    }
    camera.setResolution(CAM_RES_VGA);
    
    // Initialize SD card
    if (!sdCard.isMounted()) {
        sdCard.init();
    }
    
    // Initialize LED
    led.init();
    
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setTextSize(2);
    tft.setCursor(20, 40);
    tft.print("POV CAM");
    
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 70);
    tft.print("A: Start/Stop Recording");
    tft.setCursor(10, 85);
    tft.print("X: Toggle LED");
    tft.setCursor(10, 100);
    tft.print("BACK: Exit");
    
    if (!sdCard.isMounted()) {
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(10, 115);
        tft.print("! NO SD CARD !");
    } else {
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(10, 115);
        tft.printf("SD: %lluMB free", sdCard.getFreeBytes() / (1024 * 1024));
    }
    
    povActive = true;
    isRecording = false;
}

void povLoop() {
    if (!povActive) return;
    
    // Update status display
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 500) {
        drawPOVStatus();
        lastStatusUpdate = millis();
    }
    
    // Record frame if recording
    if (isRecording) {
        recordFrame();
    }
    
    // Handle buttons
    if (aPressedD()) {
        if (isRecording) {
            stopRecording();
        } else {
            startRecording();
        }
    }
    
    if (xPressedD()) {
        led.toggle();
    }
}

void povCleanup() {
    if (isRecording) {
        stopRecording();
    }
    povActive = false;
    Serial.println("[POV] Mode exited");
}

// ============================================================
// Mode Definition
// ============================================================
DEFINE_CAMERA_MODE(povModeDef, "POV Cam", "SD Card Video Recording",
                   povInit, povLoop, povCleanup);

// GameDef compatibility
GameDef povMode = { "POV Cam", povInit, povLoop };
