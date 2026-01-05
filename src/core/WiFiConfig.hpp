#pragma once
/**
 * @file WiFiConfig.hpp
 * @brief WiFi Configuration and Connection Manager for OpenCamX
 * 
 * Provides:
 * - WiFi credential storage (SSID + password)
 * - Auto-connect with retry logic
 * - AP mode fallback for initial setup
 * - NVS (Non-Volatile Storage) persistence
 * 
 * USAGE:
 *   wifiConfig.setCredentials("MyNetwork", "MyPassword");
 *   wifiConfig.connect();
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// ============================================================
// CONFIGURATION
// ============================================================

#define WIFI_CONNECT_TIMEOUT_MS   15000   // 15 seconds to connect
#define WIFI_RETRY_DELAY_MS       5000    // Retry every 5 seconds
#define WIFI_MAX_RETRIES          3       // Max connection attempts
#define WIFI_AP_SSID              "OpenCamX-Setup"
#define WIFI_AP_PASSWORD          "camforge123"
#define WIFI_NVS_NAMESPACE        "wifi_config"

// ============================================================
// WiFi Status Enum
// ============================================================

enum WiFiStatus {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_AP_MODE,
    WIFI_STATUS_ERROR
};

// ============================================================
// WiFi Configuration Manager
// ============================================================

class WiFiConfigManager {
private:
    Preferences _prefs;
    char _ssid[64];
    char _password[64];
    WiFiStatus _status;
    int _retryCount;
    bool _apMode;
    
public:
    WiFiConfigManager() : _status(WIFI_STATUS_DISCONNECTED), _retryCount(0), _apMode(false) {
        memset(_ssid, 0, sizeof(_ssid));
        memset(_password, 0, sizeof(_password));
    }
    
    /**
     * @brief Initialize WiFi manager and load saved credentials
     * @return true if credentials were found in NVS
     */
    bool init() {
        Serial.println("[WIFI] Initializing...");
        
        // Load saved credentials from NVS
        if (_prefs.begin(WIFI_NVS_NAMESPACE, true)) {  // Read-only
            String savedSSID = _prefs.getString("ssid", "");
            String savedPass = _prefs.getString("password", "");
            _prefs.end();
            
            if (savedSSID.length() > 0) {
                strncpy(_ssid, savedSSID.c_str(), sizeof(_ssid) - 1);
                strncpy(_password, savedPass.c_str(), sizeof(_password) - 1);
                Serial.printf("[WIFI] Loaded credentials for: %s\n", _ssid);
                return true;
            }
        }
        
        Serial.println("[WIFI] No saved credentials found");
        return false;
    }
    
    /**
     * @brief Set WiFi credentials (does NOT save to NVS yet)
     * @param ssid Network name
     * @param password Network password
     */
    void setCredentials(const char* ssid, const char* password) {
        strncpy(_ssid, ssid, sizeof(_ssid) - 1);
        strncpy(_password, password, sizeof(_password) - 1);
        Serial.printf("[WIFI] Credentials set for: %s\n", _ssid);
    }
    
    /**
     * @brief Save current credentials to NVS (persistent storage)
     * @return true on success
     */
    bool saveCredentials() {
        if (strlen(_ssid) == 0) {
            Serial.println("[WIFI] Cannot save empty credentials");
            return false;
        }
        
        if (_prefs.begin(WIFI_NVS_NAMESPACE, false)) {  // Read-write
            _prefs.putString("ssid", _ssid);
            _prefs.putString("password", _password);
            _prefs.end();
            Serial.println("[WIFI] Credentials saved to NVS");
            return true;
        }
        
        Serial.println("[WIFI] Failed to save credentials");
        return false;
    }
    
    /**
     * @brief Clear saved credentials from NVS
     */
    void clearCredentials() {
        if (_prefs.begin(WIFI_NVS_NAMESPACE, false)) {
            _prefs.clear();
            _prefs.end();
        }
        memset(_ssid, 0, sizeof(_ssid));
        memset(_password, 0, sizeof(_password));
        Serial.println("[WIFI] Credentials cleared");
    }
    
    /**
     * @brief Connect to WiFi using stored credentials
     * @return true if connected successfully
     */
    bool connect() {
        if (strlen(_ssid) == 0) {
            Serial.println("[WIFI] No SSID configured");
            startAPMode();
            return false;
        }
        
        _status = WIFI_STATUS_CONNECTING;
        _retryCount = 0;
        
        Serial.printf("[WIFI] Connecting to: %s\n", _ssid);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(_ssid, _password);
        
        // Wait for connection with timeout
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - startTime > WIFI_CONNECT_TIMEOUT_MS) {
                _retryCount++;
                Serial.printf("[WIFI] Timeout (attempt %d/%d)\n", _retryCount, WIFI_MAX_RETRIES);
                
                if (_retryCount >= WIFI_MAX_RETRIES) {
                    Serial.println("[WIFI] Max retries reached, starting AP mode");
                    startAPMode();
                    return false;
                }
                
                // Retry
                WiFi.disconnect();
                delay(1000);
                WiFi.begin(_ssid, _password);
                startTime = millis();
            }
            delay(100);
            Serial.print(".");
        }
        
        Serial.println();
        _status = WIFI_STATUS_CONNECTED;
        _apMode = false;
        
        Serial.println("[WIFI] Connected!");
        Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] Signal: %d dBm\n", WiFi.RSSI());
        
        return true;
    }
    
    /**
     * @brief Start Access Point mode for initial configuration
     * 
     * Creates a WiFi network that users can connect to
     * for setting up the device's WiFi credentials.
     */
    void startAPMode() {
        Serial.println("[WIFI] Starting AP mode...");
        
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
        
        _status = WIFI_STATUS_AP_MODE;
        _apMode = true;
        
        Serial.printf("[WIFI] AP Started: %s\n", WIFI_AP_SSID);
        Serial.printf("[WIFI] AP Password: %s\n", WIFI_AP_PASSWORD);
        Serial.printf("[WIFI] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    
    /**
     * @brief Disconnect from WiFi
     */
    void disconnect() {
        WiFi.disconnect();
        _status = WIFI_STATUS_DISCONNECTED;
        Serial.println("[WIFI] Disconnected");
    }
    
    /**
     * @brief Check if currently connected to WiFi
     */
    bool isConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }
    
    /**
     * @brief Check if in AP mode
     */
    bool isAPMode() const {
        return _apMode;
    }
    
    /**
     * @brief Get current WiFi status
     */
    WiFiStatus getStatus() const {
        return _status;
    }
    
    /**
     * @brief Get IP address (works for both STA and AP mode)
     */
    String getIPAddress() const {
        if (_apMode) {
            return WiFi.softAPIP().toString();
        }
        return WiFi.localIP().toString();
    }
    
    /**
     * @brief Get signal strength (STA mode only)
     */
    int getRSSI() const {
        return WiFi.RSSI();
    }
    
    /**
     * @brief Get configured SSID
     */
    const char* getSSID() const {
        return _ssid;
    }
    
    /**
     * @brief Scan for available networks
     * @param results Array to store network names
     * @param maxResults Maximum networks to return
     * @return Number of networks found
     */
    int scanNetworks(String* results, int maxResults) {
        Serial.println("[WIFI] Scanning networks...");
        
        int found = WiFi.scanNetworks();
        int count = min(found, maxResults);
        
        for (int i = 0; i < count; i++) {
            results[i] = WiFi.SSID(i);
            Serial.printf("[WIFI] Found: %s (%d dBm)\n", 
                         WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
        
        WiFi.scanDelete();
        return count;
    }
    
    /**
     * @brief Get status as string (for display)
     */
    const char* getStatusString() const {
        switch (_status) {
            case WIFI_STATUS_DISCONNECTED: return "Disconnected";
            case WIFI_STATUS_CONNECTING:   return "Connecting...";
            case WIFI_STATUS_CONNECTED:    return "Connected";
            case WIFI_STATUS_AP_MODE:      return "AP Mode";
            case WIFI_STATUS_ERROR:        return "Error";
            default:                       return "Unknown";
        }
    }
};

// ============================================================
// Global Instance
// ============================================================

inline WiFiConfigManager wifiConfig;
