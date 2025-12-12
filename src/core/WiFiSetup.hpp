#pragma once
/**
 * @file WiFiSetup.hpp
 * @brief WiFi Setup Screen with TFT Display + Web Portal
 * 
 * FEATURES:
 * - Shows WiFi status on ST7735 display
 * - Scans and displays available networks
 * - Web portal for entering credentials (when in AP mode)
 * - Visual feedback for connection progress
 * 
 * USAGE:
 *   Register as a mode in main.cpp:
 *   registerGame(&wifiSetupMode);
 */

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "WiFiConfig.hpp"
#include "Input.hpp"

extern Adafruit_ST7735 tft;

// ============================================================
// WEB PORTAL HTML (Captive Portal)
// ============================================================

static const char WIFI_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>OpenCamX WiFi Setup</title>
  <style>
    body{font-family:Arial;background:#1a1a2e;color:#fff;padding:20px;margin:0}
    .container{max-width:400px;margin:0 auto}
    h1{color:#00d4ff;text-align:center}
    h2{color:#888;font-size:14px;text-align:center}
    input,select{width:100%;padding:12px;margin:8px 0;border:none;border-radius:8px;
      background:#2d2d44;color:#fff;font-size:16px;box-sizing:border-box}
    button{width:100%;padding:14px;background:linear-gradient(135deg,#00d4ff,#0099cc);
      border:none;border-radius:8px;color:#fff;font-size:18px;cursor:pointer;margin-top:16px}
    button:hover{background:linear-gradient(135deg,#00b8e6,#0088b3)}
    .networks{background:#2d2d44;border-radius:8px;padding:10px;margin:10px 0}
    .net{padding:8px;border-bottom:1px solid #444;cursor:pointer}
    .net:hover{background:#3d3d55}
    .signal{float:right;color:#00d4ff}
    .status{text-align:center;padding:10px;background:#2d2d44;border-radius:8px;margin:10px 0}
    .ok{color:#00ff88}.err{color:#ff4444}
  </style>
</head>
<body>
  <div class="container">
    <h1>📷 OpenCamX</h1>
    <h2>WiFi Configuration</h2>
    <div class="status" id="status">Scanning networks...</div>
    <form action="/save" method="POST">
      <div class="networks" id="networks"></div>
      <input type="text" name="ssid" id="ssid" placeholder="Network Name (SSID)" required>
      <input type="password" name="pass" placeholder="Password">
      <button type="submit">Connect</button>
    </form>
  </div>
  <script>
    fetch('/scan').then(r=>r.json()).then(nets=>{
      let html='';
      nets.forEach(n=>{
        html+=`<div class="net" onclick="document.getElementById('ssid').value='${n.ssid}'">
          ${n.ssid}<span class="signal">${n.rssi}dBm</span></div>`;
      });
      document.getElementById('networks').innerHTML=html||'<div>No networks found</div>';
      document.getElementById('status').innerHTML='Select a network below or enter manually';
    }).catch(e=>{
      document.getElementById('status').innerHTML='<span class="err">Scan failed</span>';
    });
  </script>
</body>
</html>
)rawliteral";

static const char WIFI_SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Success!</title>
  <style>
    body{font-family:Arial;background:#1a1a2e;color:#fff;padding:40px;text-align:center}
    h1{color:#00ff88;font-size:48px}
    p{color:#888;font-size:18px}
  </style>
</head>
<body>
  <h1>✓</h1>
  <p>Credentials saved!<br>Device will now restart and connect.</p>
</body>
</html>
)rawliteral";

// ============================================================
// WiFi Setup Mode Class
// ============================================================

class WiFiSetupScreen {
private:
    WebServer* _server;
    DNSServer* _dnsServer;
    bool _portalActive;
    bool _needsRefresh;
    unsigned long _lastUpdate;
    int _animFrame;
    String _networks[10];
    int _networkCount;
    
    // Colors
    static constexpr uint16_t COLOR_BG = 0x0000;
    static constexpr uint16_t COLOR_TITLE = 0x07FF;     // Cyan
    static constexpr uint16_t COLOR_TEXT = 0xFFFF;      // White
    static constexpr uint16_t COLOR_DIM = 0x8410;       // Gray
    static constexpr uint16_t COLOR_OK = 0x07E0;        // Green
    static constexpr uint16_t COLOR_ERR = 0xF800;       // Red
    static constexpr uint16_t COLOR_WARN = 0xFFE0;      // Yellow
    
public:
    WiFiSetupScreen() : _server(nullptr), _dnsServer(nullptr), 
                        _portalActive(false), _needsRefresh(true),
                        _lastUpdate(0), _animFrame(0), _networkCount(0) {}
    
    /**
     * @brief Initialize WiFi setup screen
     */
    void init() {
        Serial.println("[WIFI-UI] Initializing setup screen...");
        
        // Initialize WiFi manager
        wifiConfig.init();
        
        // Draw initial screen
        drawScreen();
        
        // Check if we have saved credentials
        if (strlen(wifiConfig.getSSID()) > 0) {
            drawStatus("Connecting...", COLOR_WARN);
            if (wifiConfig.connect()) {
                drawStatus("Connected!", COLOR_OK);
                drawIPAddress();
            } else {
                drawStatus("Failed - AP Mode", COLOR_ERR);
                startPortal();
            }
        } else {
            drawStatus("No WiFi configured", COLOR_DIM);
            startPortal();
        }
        
        _lastUpdate = millis();
    }
    
    /**
     * @brief Main loop - handle portal requests and button input
     */
    void loop() {
        // Handle web portal requests
        if (_portalActive) {
            _dnsServer->processNextRequest();
            _server->handleClient();
            
            // Animate "AP Mode" indicator
            if (millis() - _lastUpdate > 500) {
                _lastUpdate = millis();
                _animFrame = (_animFrame + 1) % 4;
                drawAPAnimation();
            }
        }
        
        // Button handling
        if (aPressedD()) {
            // Rescan networks
            drawStatus("Scanning...", COLOR_WARN);
            scanNetworks();
            drawNetworkList();
            drawStatus("Select network", COLOR_DIM);
        }
        
        if (bPressedD()) {
            // Try to reconnect
            drawStatus("Reconnecting...", COLOR_WARN);
            if (wifiConfig.connect()) {
                drawStatus("Connected!", COLOR_OK);
                drawIPAddress();
                stopPortal();
            } else {
                drawStatus("Failed", COLOR_ERR);
            }
        }
        
        if (xPressedD()) {
            // Toggle AP mode
            if (_portalActive) {
                stopPortal();
                drawStatus("Portal stopped", COLOR_DIM);
            } else {
                startPortal();
                drawStatus("AP Mode active", COLOR_WARN);
            }
        }
    }
    
    /**
     * @brief Cleanup when exiting mode
     */
    void cleanup() {
        stopPortal();
    }
    
private:
    /**
     * @brief Draw main screen layout
     */
    void drawScreen() {
        tft.fillScreen(COLOR_BG);
        
        // Title
        tft.setTextColor(COLOR_TITLE);
        tft.setTextSize(2);
        tft.setCursor(20, 5);
        tft.print("WiFi Setup");
        
        // Divider line
        tft.drawFastHLine(0, 25, tft.width(), COLOR_DIM);
        
        // Status area (will be updated)
        drawStatus("Initializing...", COLOR_DIM);
        
        // Button hints at bottom
        tft.setTextColor(COLOR_DIM);
        tft.setTextSize(1);
        tft.setCursor(5, tft.height() - 30);
        tft.print("A:Scan B:Connect X:AP");
        tft.setCursor(5, tft.height() - 20);
        tft.print("Portal: 192.168.4.1");
    }
    
    /**
     * @brief Draw status message
     */
    void drawStatus(const char* msg, uint16_t color) {
        // Clear status area
        tft.fillRect(0, 30, tft.width(), 20, COLOR_BG);
        
        tft.setTextColor(color);
        tft.setTextSize(1);
        tft.setCursor(5, 35);
        tft.print(msg);
    }
    
    /**
     * @brief Draw current IP address
     */
    void drawIPAddress() {
        tft.fillRect(0, 50, tft.width(), 15, COLOR_BG);
        tft.setTextColor(COLOR_OK);
        tft.setTextSize(1);
        tft.setCursor(5, 52);
        tft.print("IP: ");
        tft.print(wifiConfig.getIPAddress());
    }
    
    /**
     * @brief Draw network list from scan
     */
    void drawNetworkList() {
        // Clear list area
        tft.fillRect(0, 55, tft.width(), 50, COLOR_BG);
        
        tft.setTextSize(1);
        for (int i = 0; i < min(_networkCount, 4); i++) {
            tft.setTextColor(i == 0 ? COLOR_TEXT : COLOR_DIM);
            tft.setCursor(5, 58 + i * 12);
            tft.print(_networks[i].substring(0, 18));  // Truncate long names
        }
    }
    
    /**
     * @brief Animate AP mode indicator
     */
    void drawAPAnimation() {
        const char* frames[] = {"[    ]", "[=   ]", "[==  ]", "[=== ]"};
        tft.fillRect(100, 35, 40, 10, COLOR_BG);
        tft.setTextColor(COLOR_WARN);
        tft.setTextSize(1);
        tft.setCursor(100, 35);
        tft.print(frames[_animFrame]);
    }
    
    /**
     * @brief Scan for networks
     */
    void scanNetworks() {
        _networkCount = wifiConfig.scanNetworks(_networks, 10);
    }
    
    /**
     * @brief Start captive portal for credential entry
     */
    void startPortal() {
        if (_portalActive) return;
        
        Serial.println("[WIFI-UI] Starting captive portal...");
        
        // Start AP mode
        wifiConfig.startAPMode();
        
        // DNS server for captive portal redirect
        _dnsServer = new DNSServer();
        _dnsServer->start(53, "*", WiFi.softAPIP());
        
        // Web server
        _server = new WebServer(80);
        
        // Routes
        _server->on("/", HTTP_GET, [this]() {
            _server->send_P(200, "text/html", WIFI_PORTAL_HTML);
        });
        
        _server->on("/scan", HTTP_GET, [this]() {
            scanNetworks();
            String json = "[";
            for (int i = 0; i < _networkCount; i++) {
                if (i > 0) json += ",";
                json += "{\"ssid\":\"" + _networks[i] + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
            }
            json += "]";
            _server->send(200, "application/json", json);
        });
        
        _server->on("/save", HTTP_POST, [this]() {
            String ssid = _server->arg("ssid");
            String pass = _server->arg("pass");
            
            if (ssid.length() > 0) {
                wifiConfig.setCredentials(ssid.c_str(), pass.c_str());
                wifiConfig.saveCredentials();
                _server->send_P(200, "text/html", WIFI_SUCCESS_HTML);
                
                // Schedule restart
                delay(2000);
                ESP.restart();
            } else {
                _server->send(400, "text/plain", "SSID required");
            }
        });
        
        // Captive portal redirect
        _server->onNotFound([this]() {
            _server->sendHeader("Location", "http://192.168.4.1/", true);
            _server->send(302, "text/plain", "");
        });
        
        _server->begin();
        _portalActive = true;
        
        Serial.println("[WIFI-UI] Portal active at 192.168.4.1");
        drawStatus("AP: OpenCamX-Setup", COLOR_WARN);
    }
    
    /**
     * @brief Stop captive portal
     */
    void stopPortal() {
        if (!_portalActive) return;
        
        if (_server) {
            _server->stop();
            delete _server;
            _server = nullptr;
        }
        
        if (_dnsServer) {
            _dnsServer->stop();
            delete _dnsServer;
            _dnsServer = nullptr;
        }
        
        _portalActive = false;
        Serial.println("[WIFI-UI] Portal stopped");
    }
};

// ============================================================
// MODE REGISTRATION
// ============================================================

static WiFiSetupScreen wifiSetupScreen;

void wifiSetupInit() {
    wifiSetupScreen.init();
}

void wifiSetupLoop() {
    wifiSetupScreen.loop();
}

void wifiSetupCleanup() {
    wifiSetupScreen.cleanup();
}

// Game definition for menu registration
inline GameDef wifiSetupMode = { "WiFi Setup", wifiSetupInit, wifiSetupLoop };
