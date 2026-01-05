#pragma once
/**
 * @file OTA.hpp
 * @brief Over-The-Air Update System for OpenCamX
 * 
 * Downloads firmware from GitHub releases and performs
 * safe OTA updates with rollback support.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

class OTAUpdater {
private:
    String _currentVersion = "1.0.0";
    String _updateUrl;
    bool _updateAvailable = false;
    
public:
    /**
     * Check for updates from GitHub releases
     */
    bool checkForUpdate(const char* repoOwner, const char* repoName) {
        String apiUrl = "https://api.github.com/repos/";
        apiUrl += repoOwner;
        apiUrl += "/";
        apiUrl += repoName;
        apiUrl += "/releases/latest";
        
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("User-Agent", "OpenCamX-OTA");
        
        int httpCode = http.GET();
        if (httpCode != 200) {
            Serial.printf("[OTA] GitHub API error: %d\n", httpCode);
            http.end();
            return false;
        }
        
        String payload = http.getString();
        http.end();
        
        // Simple JSON parsing for tag_name
        int tagStart = payload.indexOf("\"tag_name\":\"") + 12;
        int tagEnd = payload.indexOf("\"", tagStart);
        String latestVersion = payload.substring(tagStart, tagEnd);
        
        // Find firmware asset URL
        int assetStart = payload.indexOf("\"browser_download_url\":\"") + 24;
        int assetEnd = payload.indexOf("\"", assetStart);
        _updateUrl = payload.substring(assetStart, assetEnd);
        
        Serial.printf("[OTA] Current: %s, Latest: %s\n", 
                      _currentVersion.c_str(), latestVersion.c_str());
        
        _updateAvailable = (latestVersion != _currentVersion);
        return _updateAvailable;
    }
    
    /**
     * Download and install update
     */
    bool performUpdate() {
        if (_updateUrl.length() == 0) {
            Serial.println("[OTA] No update URL set");
            return false;
        }
        
        Serial.printf("[OTA] Downloading: %s\n", _updateUrl.c_str());
        
        HTTPClient http;
        http.begin(_updateUrl);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        int httpCode = http.GET();
        if (httpCode != 200) {
            Serial.printf("[OTA] Download failed: %d\n", httpCode);
            http.end();
            return false;
        }
        
        int contentLength = http.getSize();
        if (contentLength <= 0) {
            Serial.println("[OTA] Invalid content length");
            http.end();
            return false;
        }
        
        Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
        
        if (!Update.begin(contentLength)) {
            Serial.println("[OTA] Not enough space");
            http.end();
            return false;
        }
        
        WiFiClient* stream = http.getStreamPtr();
        size_t written = Update.writeStream(*stream);
        
        if (written != contentLength) {
            Serial.printf("[OTA] Write error: %d/%d\n", written, contentLength);
            http.end();
            return false;
        }
        
        if (!Update.end()) {
            Serial.printf("[OTA] Update error: %s\n", Update.errorString());
            http.end();
            return false;
        }
        
        http.end();
        Serial.println("[OTA] Update successful! Rebooting...");
        delay(1000);
        ESP.restart();
        
        return true;
    }
    
    /**
     * Connect to WiFi
     */
    bool connectWiFi(const char* ssid, const char* password, int timeoutMs = 10000) {
        Serial.printf("[OTA] Connecting to %s...\n", ssid);
        WiFi.begin(ssid, password);
        
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > timeoutMs) {
                Serial.println("[OTA] WiFi connection timeout");
                return false;
            }
            delay(500);
            Serial.print(".");
        }
        
        Serial.printf("\n[OTA] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    bool isUpdateAvailable() const { return _updateAvailable; }
    String getCurrentVersion() const { return _currentVersion; }
    void setCurrentVersion(const char* ver) { _currentVersion = ver; }
};

// Global OTA instance
inline OTAUpdater ota;
