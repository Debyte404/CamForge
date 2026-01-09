#pragma once
/**
 * @file OTAWebUI.hpp
 * @brief Web Dashboard for OTA Management
 * 
 * Provides a beautiful web interface for:
 * - Viewing current/latest firmware versions
 * - Manually checking for updates
 * - Triggering firmware updates
 * - Real-time progress display
 * 
 * USAGE:
 *   otaWebUI.init(80);  // Start on port 80
 *   // In loop:
 *   otaWebUI.loop();
 */

#include <Arduino.h>
#include <WebServer.h>
#include "OTA.hpp"

// ============================================================
// OTA WEB DASHBOARD HTML
// ============================================================

static const char OTA_DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>CamForge OTA</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);
      color:#fff;min-height:100vh;padding:20px}
    .container{max-width:500px;margin:0 auto}
    h1{text-align:center;font-size:28px;margin-bottom:8px;
      background:linear-gradient(90deg,#00d4ff,#00ff88);-webkit-background-clip:text;
      -webkit-text-fill-color:transparent}
    .subtitle{text-align:center;color:#888;font-size:14px;margin-bottom:24px}
    .card{background:rgba(255,255,255,0.05);border-radius:16px;padding:24px;margin-bottom:16px;
      border:1px solid rgba(255,255,255,0.1);backdrop-filter:blur(10px)}
    .version-row{display:flex;justify-content:space-between;align-items:center;padding:12px 0;
      border-bottom:1px solid rgba(255,255,255,0.1)}
    .version-row:last-child{border-bottom:none}
    .label{color:#888;font-size:14px}
    .value{font-size:18px;font-weight:600}
    .value.current{color:#00d4ff}
    .value.latest{color:#00ff88}
    .value.update{color:#ffcc00}
    .status{text-align:center;padding:16px;background:rgba(0,212,255,0.1);border-radius:12px;margin:16px 0}
    .status.error{background:rgba(255,68,68,0.2);color:#ff4444}
    .status.success{background:rgba(0,255,136,0.2);color:#00ff88}
    .btn{width:100%;padding:16px;border:none;border-radius:12px;font-size:16px;font-weight:600;
      cursor:pointer;transition:all 0.3s;margin-top:12px}
    .btn-primary{background:linear-gradient(135deg,#00d4ff,#0099cc);color:#fff}
    .btn-primary:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(0,212,255,0.3)}
    .btn-success{background:linear-gradient(135deg,#00ff88,#00cc6a);color:#1a1a2e}
    .btn-success:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(0,255,136,0.3)}
    .btn:disabled{opacity:0.5;cursor:not-allowed;transform:none}
    .progress-container{display:none;margin-top:16px}
    .progress-bar{height:8px;background:rgba(255,255,255,0.1);border-radius:4px;overflow:hidden}
    .progress-fill{height:100%;background:linear-gradient(90deg,#00d4ff,#00ff88);width:0%;
      transition:width 0.3s}
    .progress-text{text-align:center;margin-top:8px;font-size:14px;color:#888}
    .notes{background:rgba(0,0,0,0.2);border-radius:8px;padding:12px;margin-top:16px;
      font-size:13px;color:#aaa;max-height:120px;overflow-y:auto}
    .notes-title{font-size:12px;color:#666;margin-bottom:8px}
    @keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
    .checking{animation:pulse 1.5s infinite}
  </style>
</head>
<body>
  <div class="container">
    <h1>📸 CamForge</h1>
    <p class="subtitle">Firmware Update Manager</p>
    
    <div class="card">
      <div class="version-row">
        <span class="label">Current Version</span>
        <span class="value current" id="current">Loading...</span>
      </div>
      <div class="version-row">
        <span class="label">Latest Version</span>
        <span class="value latest" id="latest">-</span>
      </div>
      <div class="version-row">
        <span class="label">Status</span>
        <span class="value" id="statusVal">-</span>
      </div>
    </div>
    
    <div class="status" id="status">Ready</div>
    
    <div class="progress-container" id="progressContainer">
      <div class="progress-bar"><div class="progress-fill" id="progressFill"></div></div>
      <div class="progress-text" id="progressText">0%</div>
    </div>
    
    <button class="btn btn-primary" id="checkBtn" onclick="checkUpdate()">Check for Updates</button>
    <button class="btn btn-success" id="updateBtn" onclick="startUpdate()" style="display:none">Install Update</button>
    
    <div class="notes" id="notes" style="display:none">
      <div class="notes-title">Release Notes:</div>
      <div id="notesContent"></div>
    </div>
  </div>
  
  <script>
    let pollInterval;
    
    async function fetchStatus() {
      try {
        const res = await fetch('/api/ota/status');
        const data = await res.json();
        document.getElementById('current').textContent = data.current;
        document.getElementById('latest').textContent = data.latest || '-';
        document.getElementById('statusVal').textContent = data.statusText;
        
        if (data.updateAvailable) {
          document.getElementById('updateBtn').style.display = 'block';
          document.getElementById('statusVal').className = 'value update';
          if (data.notes) {
            document.getElementById('notes').style.display = 'block';
            document.getElementById('notesContent').textContent = data.notes;
          }
        }
        
        if (data.status >= 3 && data.status <= 4) { // Downloading/Installing
          document.getElementById('progressContainer').style.display = 'block';
          document.getElementById('checkBtn').disabled = true;
          document.getElementById('updateBtn').disabled = true;
        }
      } catch (e) {
        console.error('Status fetch failed:', e);
      }
    }
    
    async function checkUpdate() {
      const btn = document.getElementById('checkBtn');
      const status = document.getElementById('status');
      btn.disabled = true;
      btn.textContent = 'Checking...';
      status.className = 'status checking';
      status.textContent = 'Contacting GitHub...';
      
      try {
        const res = await fetch('/api/ota/check', { method: 'POST' });
        const data = await res.json();
        
        if (data.updateAvailable) {
          status.className = 'status success';
          status.textContent = 'Update available: ' + data.latest;
          document.getElementById('updateBtn').style.display = 'block';
        } else {
          status.className = 'status';
          status.textContent = 'You have the latest version!';
        }
        fetchStatus();
      } catch (e) {
        status.className = 'status error';
        status.textContent = 'Check failed: ' + e.message;
      }
      
      btn.disabled = false;
      btn.textContent = 'Check for Updates';
    }
    
    async function startUpdate() {
      if (!confirm('Install update? The device will reboot.')) return;
      
      document.getElementById('checkBtn').disabled = true;
      document.getElementById('updateBtn').disabled = true;
      document.getElementById('progressContainer').style.display = 'block';
      document.getElementById('status').textContent = 'Starting update...';
      
      // Start polling for progress
      pollInterval = setInterval(async () => {
        try {
          const res = await fetch('/api/ota/progress');
          const data = await res.json();
          document.getElementById('progressFill').style.width = data.percent + '%';
          document.getElementById('progressText').textContent = data.message;
          document.getElementById('status').textContent = data.message;
        } catch (e) {}
      }, 500);
      
      try {
        await fetch('/api/ota/update', { method: 'POST' });
      } catch (e) {
        // Connection will drop on reboot
      }
    }
    
    // Initial load
    fetchStatus();
  </script>
</body>
</html>
)rawliteral";

// ============================================================
// OTA WEB UI CLASS
// ============================================================

class OTAWebUI {
private:
    WebServer* _server;
    bool _active;
    int _lastPercent;
    String _lastMessage;
    
public:
    OTAWebUI() : _server(nullptr), _active(false), _lastPercent(0), _lastMessage("Ready") {}
    
    /**
     * @brief Initialize OTA Web UI
     * @param port HTTP port (default 80)
     */
    void init(uint16_t port = 80) {
        if (_active) return;
        
        _server = new WebServer(port);
        
        // Main dashboard
        _server->on("/ota", HTTP_GET, [this]() {
            _server->send_P(200, "text/html", OTA_DASHBOARD_HTML);
        });
        
        // Status API
        _server->on("/api/ota/status", HTTP_GET, [this]() {
            String json = "{";
            json += "\"current\":\"" + otaManager.getCurrentVersion() + "\",";
            json += "\"latest\":\"" + otaManager.getLatestVersion() + "\",";
            json += "\"status\":" + String((int)otaManager.getStatus()) + ",";
            json += "\"statusText\":\"" + String(otaManager.getStatusString()) + "\",";
            json += "\"updateAvailable\":" + String(otaManager.isUpdateAvailable() ? "true" : "false") + ",";
            json += "\"notes\":\"" + escapeJson(otaManager.getReleaseNotes()) + "\"";
            json += "}";
            _server->send(200, "application/json", json);
        });
        
        // Check for update
        _server->on("/api/ota/check", HTTP_POST, [this]() {
            bool available = otaManager.checkForUpdate();
            String json = "{";
            json += "\"updateAvailable\":" + String(available ? "true" : "false") + ",";
            json += "\"latest\":\"" + otaManager.getLatestVersion() + "\"";
            json += "}";
            _server->send(200, "application/json", json);
        });
        
        // Progress API
        _server->on("/api/ota/progress", HTTP_GET, [this]() {
            String json = "{";
            json += "\"percent\":" + String(_lastPercent) + ",";
            json += "\"message\":\"" + _lastMessage + "\"";
            json += "}";
            _server->send(200, "application/json", json);
        });
        
        // Start update
        _server->on("/api/ota/update", HTTP_POST, [this]() {
            _server->send(200, "application/json", "{\"status\":\"started\"}");
            delay(100);  // Let response send
            otaManager.performUpdate();  // Will reboot on success
        });
        
        // Set progress callback to track updates
        otaManager.setProgressCallback([](int percent, const char* status) {
            // Store for progress API (can't capture 'this' in static lambda)
            Serial.printf("[OTA-UI] %d%% - %s\n", percent, status);
        });
        
        _server->begin();
        _active = true;
        
        Serial.printf("[OTA-UI] Web dashboard running at /ota\n");
    }
    
    /**
     * @brief Handle web requests - call from main loop
     */
    void loop() {
        if (_active && _server) {
            _server->handleClient();
        }
    }
    
    /**
     * @brief Stop web server
     */
    void stop() {
        if (_server) {
            _server->stop();
            delete _server;
            _server = nullptr;
        }
        _active = false;
    }
    
    bool isActive() const { return _active; }
    
private:
    /**
     * @brief Escape JSON special characters
     */
    String escapeJson(const String& input) {
        String output;
        output.reserve(input.length());
        for (size_t i = 0; i < input.length(); i++) {
            char c = input.charAt(i);
            switch (c) {
                case '"':  output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:   output += c;
            }
        }
        return output;
    }
};

// ============================================================
// GLOBAL INSTANCE
// ============================================================

inline OTAWebUI otaWebUI;
