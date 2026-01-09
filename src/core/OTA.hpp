#pragma once
/**
 * @file OTA.hpp
 * @brief Hyper-Advanced OTA Update System for CamForge
 * 
 * FEATURES:
 * - Automatic GitHub Release checking
 * - Semantic version comparison (vX.Y.Z)
 * - NVS-persistent version storage
 * - Non-blocking background updates
 * - Progress callbacks for UI integration
 * - HTTPS support for secure downloads
 * 
 * USAGE:
 *   otaManager.init("Debyte404", "CamForge");
 *   otaManager.setProgressCallback(myCallback);
 *   // In loop:
 *   otaManager.loop();
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ============================================================
// CONFIGURATION
// ============================================================

#define OTA_CHECK_INTERVAL_MS     7200000   // 2 hours between checks
#define OTA_DOWNLOAD_TIMEOUT_MS   60000     // 60 second download timeout
#define OTA_NVS_NAMESPACE         "ota_mgr"
#define OTA_USER_AGENT            "CamForge-OTA/2.0"
#define OTA_FIRMWARE_VERSION      "1.0.0"   // Default if not set in NVS

// ============================================================
// OTA STATUS CODES
// ============================================================

enum OTAStatus {
    OTA_IDLE = 0,
    OTA_CHECKING,
    OTA_UPDATE_AVAILABLE,
    OTA_DOWNLOADING,
    OTA_INSTALLING,
    OTA_SUCCESS,
    OTA_ERROR_NO_WIFI,
    OTA_ERROR_API_FAILED,
    OTA_ERROR_PARSE_FAILED,
    OTA_ERROR_DOWNLOAD_FAILED,
    OTA_ERROR_UPDATE_FAILED,
    OTA_ERROR_NO_SPACE
};

// ============================================================
// VERSION STRUCTURE
// ============================================================

struct SemVer {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    
    SemVer() : major(0), minor(0), patch(0) {}
    SemVer(uint16_t maj, uint16_t min, uint16_t pat) 
        : major(maj), minor(min), patch(pat) {}
    
    /**
     * @brief Parse version string "vX.Y.Z" or "X.Y.Z"
     */
    bool parse(const char* str) {
        const char* p = str;
        if (*p == 'v' || *p == 'V') p++;  // Skip 'v' prefix
        
        int vals[3] = {0, 0, 0};
        int idx = 0;
        
        while (*p && idx < 3) {
            if (*p >= '0' && *p <= '9') {
                vals[idx] = vals[idx] * 10 + (*p - '0');
            } else if (*p == '.') {
                idx++;
            } else {
                break;  // Stop at non-version chars (e.g., "-beta")
            }
            p++;
        }
        
        major = vals[0];
        minor = vals[1];
        patch = vals[2];
        return (major > 0 || minor > 0 || patch > 0);
    }
    
    /**
     * @brief Compare versions: returns >0 if this > other
     */
    int compare(const SemVer& other) const {
        if (major != other.major) return major - other.major;
        if (minor != other.minor) return minor - other.minor;
        return patch - other.patch;
    }
    
    String toString() const {
        return "v" + String(major) + "." + String(minor) + "." + String(patch);
    }
};

// ============================================================
// CALLBACK TYPES
// ============================================================

typedef void (*OTAProgressCallback)(int percent, const char* status);
typedef void (*OTAStatusCallback)(OTAStatus status);

// ============================================================
// OTA MANAGER CLASS
// ============================================================

class OTAManager {
private:
    Preferences _prefs;
    char _repoOwner[64];
    char _repoName[64];
    SemVer _currentVersion;
    SemVer _latestVersion;
    String _updateUrl;
    String _releaseNotes;
    OTAStatus _status;
    unsigned long _lastCheckTime;
    unsigned long _checkInterval;
    bool _initialized;
    bool _updateAvailable;
    
    // Callbacks
    OTAProgressCallback _progressCallback;
    OTAStatusCallback _statusCallback;
    
    /**
     * @brief Load version from NVS
     */
    void loadVersion() {
        if (_prefs.begin(OTA_NVS_NAMESPACE, true)) {
            String ver = _prefs.getString("version", OTA_FIRMWARE_VERSION);
            _prefs.end();
            _currentVersion.parse(ver.c_str());
            Serial.printf("[OTA] Loaded version: %s\n", _currentVersion.toString().c_str());
        }
    }
    
    /**
     * @brief Save version to NVS
     */
    void saveVersion(const SemVer& ver) {
        if (_prefs.begin(OTA_NVS_NAMESPACE, false)) {
            _prefs.putString("version", ver.toString());
            _prefs.end();
            _currentVersion = ver;
            Serial.printf("[OTA] Saved version: %s\n", ver.toString().c_str());
        }
    }
    
    /**
     * @brief Report progress via callback
     */
    void reportProgress(int percent, const char* status) {
        if (_progressCallback) {
            _progressCallback(percent, status);
        }
        Serial.printf("[OTA] %s (%d%%)\n", status, percent);
    }
    
    /**
     * @brief Set status and notify callback
     */
    void setStatus(OTAStatus status) {
        _status = status;
        if (_statusCallback) {
            _statusCallback(status);
        }
    }

public:
    OTAManager() : 
        _status(OTA_IDLE),
        _lastCheckTime(0),
        _checkInterval(OTA_CHECK_INTERVAL_MS),
        _initialized(false),
        _updateAvailable(false),
        _progressCallback(nullptr),
        _statusCallback(nullptr) {
        memset(_repoOwner, 0, sizeof(_repoOwner));
        memset(_repoName, 0, sizeof(_repoName));
    }
    
    /**
     * @brief Initialize OTA manager
     * @param repoOwner GitHub repository owner
     * @param repoName GitHub repository name
     * @return true on success
     */
    bool init(const char* repoOwner, const char* repoName) {
        strncpy(_repoOwner, repoOwner, sizeof(_repoOwner) - 1);
        strncpy(_repoName, repoName, sizeof(_repoName) - 1);
        
        loadVersion();
        
        _initialized = true;
        _lastCheckTime = 0;  // Force check on first loop
        
        Serial.printf("[OTA] Initialized for %s/%s\n", _repoOwner, _repoName);
        Serial.printf("[OTA] Current version: %s\n", _currentVersion.toString().c_str());
        
        return true;
    }
    
    /**
     * @brief Set current version (for initial setup)
     */
    void setCurrentVersion(const char* version) {
        SemVer ver;
        if (ver.parse(version)) {
            saveVersion(ver);
        }
    }
    
    /**
     * @brief Set check interval
     * @param intervalMs Milliseconds between automatic checks
     */
    void setCheckInterval(unsigned long intervalMs) {
        _checkInterval = intervalMs;
    }
    
    /**
     * @brief Set progress callback
     */
    void setProgressCallback(OTAProgressCallback callback) {
        _progressCallback = callback;
    }
    
    /**
     * @brief Set status callback
     */
    void setStatusCallback(OTAStatusCallback callback) {
        _statusCallback = callback;
    }
    
    /**
     * @brief Non-blocking loop - call from main loop()
     */
    void loop() {
        if (!_initialized) return;
        if (WiFi.status() != WL_CONNECTED) return;
        
        // Periodic update check
        if (millis() - _lastCheckTime > _checkInterval || _lastCheckTime == 0) {
            checkForUpdate();
            _lastCheckTime = millis();
        }
    }
    
    /**
     * @brief Force an immediate update check
     * @return true if update is available
     */
    bool checkForUpdate() {
        if (WiFi.status() != WL_CONNECTED) {
            setStatus(OTA_ERROR_NO_WIFI);
            return false;
        }
        
        setStatus(OTA_CHECKING);
        reportProgress(0, "Checking GitHub releases...");
        
        // Build GitHub API URL
        String apiUrl = "https://api.github.com/repos/";
        apiUrl += _repoOwner;
        apiUrl += "/";
        apiUrl += _repoName;
        apiUrl += "/releases/latest";
        
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("User-Agent", OTA_USER_AGENT);
        http.addHeader("Accept", "application/vnd.github.v3+json");
        http.setTimeout(10000);
        
        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] GitHub API error: %d\n", httpCode);
            http.end();
            setStatus(OTA_ERROR_API_FAILED);
            return false;
        }
        
        String payload = http.getString();
        http.end();
        
        // Parse JSON response using ArduinoJson
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.printf("[OTA] JSON parse error: %s\n", error.c_str());
            setStatus(OTA_ERROR_PARSE_FAILED);
            return false;
        }
        
        // Extract version tag
        const char* tagName = doc["tag_name"];
        if (!tagName) {
            Serial.println("[OTA] No tag_name in response");
            setStatus(OTA_ERROR_PARSE_FAILED);
            return false;
        }
        
        _latestVersion.parse(tagName);
        
        // Extract release notes
        const char* body = doc["body"];
        if (body) {
            _releaseNotes = String(body).substring(0, 500);  // Limit size
        }
        
        // Find firmware asset (.bin file)
        _updateUrl = "";
        JsonArray assets = doc["assets"];
        for (JsonObject asset : assets) {
            const char* name = asset["name"];
            if (name && strstr(name, ".bin")) {
                const char* url = asset["browser_download_url"];
                if (url) {
                    _updateUrl = String(url);
                    break;
                }
            }
        }
        
        // Compare versions
        _updateAvailable = (_latestVersion.compare(_currentVersion) > 0);
        
        Serial.printf("[OTA] Current: %s, Latest: %s\n", 
                     _currentVersion.toString().c_str(),
                     _latestVersion.toString().c_str());
        Serial.printf("[OTA] Update available: %s\n", _updateAvailable ? "YES" : "NO");
        
        if (_updateAvailable && _updateUrl.length() > 0) {
            setStatus(OTA_UPDATE_AVAILABLE);
            reportProgress(100, "Update available!");
        } else {
            setStatus(OTA_IDLE);
            reportProgress(100, "Up to date");
        }
        
        return _updateAvailable;
    }
    
    /**
     * @brief Download and install the update
     * @return true on success (device will reboot)
     */
    bool performUpdate() {
        if (!_updateAvailable || _updateUrl.length() == 0) {
            Serial.println("[OTA] No update available or URL missing");
            return false;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            setStatus(OTA_ERROR_NO_WIFI);
            return false;
        }
        
        setStatus(OTA_DOWNLOADING);
        reportProgress(0, "Downloading firmware...");
        
        HTTPClient http;
        http.begin(_updateUrl);
        http.addHeader("User-Agent", OTA_USER_AGENT);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(OTA_DOWNLOAD_TIMEOUT_MS);
        
        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] Download failed: %d\n", httpCode);
            http.end();
            setStatus(OTA_ERROR_DOWNLOAD_FAILED);
            return false;
        }
        
        int contentLength = http.getSize();
        if (contentLength <= 0) {
            Serial.println("[OTA] Invalid content length");
            http.end();
            setStatus(OTA_ERROR_DOWNLOAD_FAILED);
            return false;
        }
        
        Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
        
        // Check if we have enough space
        if (!Update.begin(contentLength)) {
            Serial.println("[OTA] Not enough space for update");
            http.end();
            setStatus(OTA_ERROR_NO_SPACE);
            return false;
        }
        
        setStatus(OTA_INSTALLING);
        
        // Stream the update with progress reporting
        WiFiClient* stream = http.getStreamPtr();
        uint8_t buffer[1024];
        size_t totalWritten = 0;
        int lastPercent = 0;
        
        while (http.connected() && totalWritten < contentLength) {
            size_t available = stream->available();
            if (available) {
                size_t toRead = min(available, sizeof(buffer));
                size_t bytesRead = stream->readBytes(buffer, toRead);
                
                if (bytesRead > 0) {
                    size_t written = Update.write(buffer, bytesRead);
                    if (written != bytesRead) {
                        Serial.println("[OTA] Write error");
                        Update.abort();
                        http.end();
                        setStatus(OTA_ERROR_UPDATE_FAILED);
                        return false;
                    }
                    totalWritten += written;
                    
                    // Report progress
                    int percent = (totalWritten * 100) / contentLength;
                    if (percent != lastPercent) {
                        lastPercent = percent;
                        char msg[32];
                        snprintf(msg, sizeof(msg), "Installing: %d%%", percent);
                        reportProgress(percent, msg);
                    }
                }
            }
            yield();  // Allow background tasks
        }
        
        http.end();
        
        if (totalWritten != contentLength) {
            Serial.printf("[OTA] Size mismatch: %d/%d\n", totalWritten, contentLength);
            Update.abort();
            setStatus(OTA_ERROR_UPDATE_FAILED);
            return false;
        }
        
        if (!Update.end(true)) {
            Serial.printf("[OTA] Update failed: %s\n", Update.errorString());
            setStatus(OTA_ERROR_UPDATE_FAILED);
            return false;
        }
        
        // Save new version to NVS
        saveVersion(_latestVersion);
        
        setStatus(OTA_SUCCESS);
        reportProgress(100, "Update complete! Rebooting...");
        
        Serial.println("[OTA] Update successful! Rebooting...");
        delay(1500);
        ESP.restart();
        
        return true;  // Won't reach here
    }
    
    // ============================================================
    // GETTERS
    // ============================================================
    
    bool isInitialized() const { return _initialized; }
    bool isUpdateAvailable() const { return _updateAvailable; }
    OTAStatus getStatus() const { return _status; }
    String getCurrentVersion() const { return _currentVersion.toString(); }
    String getLatestVersion() const { return _latestVersion.toString(); }
    String getReleaseNotes() const { return _releaseNotes; }
    String getUpdateUrl() const { return _updateUrl; }
    
    /**
     * @brief Get status as human-readable string
     */
    const char* getStatusString() const {
        switch (_status) {
            case OTA_IDLE:               return "Idle";
            case OTA_CHECKING:           return "Checking...";
            case OTA_UPDATE_AVAILABLE:   return "Update Available";
            case OTA_DOWNLOADING:        return "Downloading...";
            case OTA_INSTALLING:         return "Installing...";
            case OTA_SUCCESS:            return "Success";
            case OTA_ERROR_NO_WIFI:      return "No WiFi";
            case OTA_ERROR_API_FAILED:   return "API Error";
            case OTA_ERROR_PARSE_FAILED: return "Parse Error";
            case OTA_ERROR_DOWNLOAD_FAILED: return "Download Failed";
            case OTA_ERROR_UPDATE_FAILED: return "Update Failed";
            case OTA_ERROR_NO_SPACE:     return "No Space";
            default:                     return "Unknown";
        }
    }
};

// ============================================================
// GLOBAL INSTANCE
// ============================================================

inline OTAManager otaManager;
