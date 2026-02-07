#include "network_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>

NetworkManager DataManager;

void NetworkManager::init() {
  _prefs.begin("sc01-pref", false);
  loadSettings();

  WiFi.mode(WIFI_STA);

  if (_ssid.length() > 0) {
    Serial.printf("Auto-connecting to: %s\n", _ssid.c_str());
    WiFi.begin(_ssid.c_str(), _password.c_str());
    _status = "Connecting...";
  }
}

void NetworkManager::loop() {
  static wl_status_t lastStatus = WL_IDLE_STATUS;
  wl_status_t currentStatus = WiFi.status();

  if (currentStatus != lastStatus) {
    lastStatus = currentStatus;
    Serial.printf("WiFi Status Change: %d\n", currentStatus);
    if (currentStatus == WL_CONNECTED) {
      Serial.print("WiFi Connected! IP: ");
      Serial.println(WiFi.localIP());
      _status = "Connected";
      beginWebServer(); // Start server on connection
    } else if (currentStatus == WL_CONNECT_FAILED ||
               currentStatus == WL_NO_SSID_AVAIL) {
      _status = "Failed";
    }
  }

  if (currentStatus == WL_CONNECTED) {
    if (!_ntpStarted) {
      configTime(_gmtOffset, 0, _ntpServer.c_str());
      _ntpStarted = true;
      log(("NTP Sync Started: " + _ntpServer).c_str());
    }
    handleWebServer();

    // Skip all other background operations during OTA upload
    // to prevent interference with flash write operations
    if (_otaInProgress) {
      return;
    }

    // Adaptive polling: slower when offline to reduce blocking impact
    uint32_t pollInterval = (_status == "Offline") ? 5000 : _pollInterval;
    if (millis() - _lastUpdate > pollInterval) {
      updatePrinterStatus();
      _lastUpdate = millis();
    }
  }
}

void NetworkManager::loadSettings() {
  _ssid = _prefs.getString("ssid", "");
  _password = _prefs.getString("pass", "");
  _printerIP = _prefs.getString("rip", "");
  _ipResolved = false; // Force re-resolution
  _pollInterval = _prefs.getUInt("poll", 5000);
  _ntpServer = _prefs.getString("ntp", "pool.ntp.org");
  _gmtOffset = _prefs.getLong("gmto", 0);
  _selectedTool = _prefs.getInt("tlidx", 0);
  _activeAFCUnit = _prefs.getInt("afcunit", 0);
  Serial.println("Settings Loaded.");
}

void NetworkManager::saveSettings() {
  _prefs.putString("ssid", _ssid);
  _prefs.putString("pass", _password);
  _prefs.putString("rip", _printerIP);
  _ipResolved = false; // Force re-resolution
  _prefs.putUInt("poll", _pollInterval);
  _prefs.putString("ntp", _ntpServer);
  _prefs.putLong("gmto", _gmtOffset);
  _prefs.putInt("tlidx", _selectedTool);
  _prefs.putInt("afcunit", _activeAFCUnit);
  Serial.println("Settings Saved.");
}

void NetworkManager::connectWiFi(const char *ssid, const char *password) {
  _ssid = ssid;
  _password = password;
  saveSettings();

  Serial.printf("Connecting to WiFi: %s\n", ssid);
  _status = "Connecting...";
  WiFi.disconnect();
  WiFi.begin(ssid, password);
}

bool NetworkManager::isConnected() { return WiFi.status() == WL_CONNECTED; }

String NetworkManager::getIP() { return WiFi.localIP().toString(); }

void NetworkManager::setPrinterIP(const char *ip) {
  _printerIP = ip;
  saveSettings();
}

void NetworkManager::log(const char *msg) {
  String timeStr = getFormattedTime();
  String entry = "[" + timeStr + "] " + String(msg);
  _logs.push_back(entry);
  if (_logs.size() > 50)
    _logs.pop_front();
  Serial.println(entry);
}

String NetworkManager::getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String(millis());
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

void NetworkManager::beginWebServer() {
  _server.on("/", HTTP_GET, [this]() {
    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                  "content='width=device-width,initial-scale=1'>";
    html += "<title>SC01+ Config v" FIRMWARE_VERSION "</title>";

    // Modern CSS with glassmorphism and card-based layout
    html += "<style>";
    html += "*{margin:0;padding:0;box-sizing:border-box;}";
    html += "body{font-family:'Segoe "
            "UI',Tahoma,sans-serif;background:linear-gradient(135deg,#0f0f1e "
            "0%,#1a1a2e 100%);color:#fff;padding:20px;min-height:100vh;}";
    html += ".container{max-width:1200px;margin:0 auto;}";

    // Header
    html += ".header{text-align:center;margin-bottom:40px;}";
    html += ".header "
            "h1{font-size:2.5em;margin-bottom:10px;background:linear-gradient("
            "135deg,#667eea 0%,#764ba2 "
            "100%);-webkit-background-clip:text;-webkit-text-fill-color:"
            "transparent;background-clip:text;}";
    html += ".badge{display:inline-block;background:rgba(102,126,234,0.2);"
            "border:1px solid rgba(102,126,234,0.4);padding:5px "
            "15px;border-radius:20px;font-size:0.9em;margin-top:10px;}";
    html += ".status{display:inline-block;margin-left:10px;}";
    html += ".status-dot{display:inline-block;width:8px;height:8px;background:#"
            "4ade80;border-radius:50%;margin-right:5px;animation:pulse 2s "
            "infinite;}";
    html += "@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.5;}}";

    // Card grid
    html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax("
            "500px,1fr));gap:25px;margin-bottom:25px;}";
    html += "@media(max-width:768px){.grid{grid-template-columns:1fr;}}";

    // Card styling with glassmorphism
    html +=
        ".card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);"
        "border:1px solid "
        "rgba(255,255,255,0.1);border-radius:16px;padding:25px;box-shadow:0 "
        "8px 32px rgba(0,0,0,0.3);transition:transform 0.3s,box-shadow 0.3s;}";
    html += ".card:hover{transform:translateY(-5px);box-shadow:0 12px 40px "
            "rgba(102,126,234,0.3);}";
    html += ".card-title{font-size:1.3em;margin-bottom:25px;display:flex;align-"
            "items:center;gap:10px;color:#667eea;}";
    html += ".card-icon{font-size:1.5em;}";

    // Form inputs
    html +=
        "label{display:block;margin-bottom:8px;font-size:0.9em;color:#a0aec0;}";
    html += "input,select{width:100%;padding:12px;margin-bottom:15px;"
            "background:rgba(255,255,255,0.08);border:1px solid "
            "rgba(255,255,255,0.15);border-radius:8px;color:#fff;font-size:1em;"
            "transition:all 0.3s;}";
    html +=
        "input:focus,select:focus{outline:none;border-color:#667eea;background:"
        "rgba(255,255,255,0.12);box-shadow:0 0 0 3px rgba(102,126,234,0.2);}";
    html += "input[type='file']{padding:10px;cursor:pointer;}";
    html += "option{background:#1a1a2e;color:#fff;}";

    // Buttons
    html += "button{padding:12px "
            "24px;background:linear-gradient(135deg,#667eea 0%,#764ba2 "
            "100%);color:#fff;border:none;border-radius:8px;cursor:pointer;"
            "font-size:1em;font-weight:600;transition:all 0.3s;box-shadow:0 "
            "4px 15px rgba(102,126,234,0.4);}";
    html += "button:hover{transform:translateY(-2px);box-shadow:0 6px 20px "
            "rgba(102,126,234,0.6);}";
    html += "button:active{transform:translateY(0);}";
    html += ".btn-secondary{background:rgba(255,255,255,0.1);box-shadow:none;}";
    html += ".btn-secondary:hover{background:rgba(255,255,255,0.15);}";
    html += ".btn-small{padding:6px 12px;font-size:0.85em;}";

    // Console
    html += "#console{background:rgba(0,0,0,0.5);color:#4ade80;padding:15px;"
            "height:250px;overflow-y:auto;font-family:'Courier "
            "New',monospace;font-size:0.9em;border-radius:8px;border:1px solid "
            "rgba(255,255,255,0.1);line-height:1.6;}";
    html += "#console::-webkit-scrollbar{width:8px;}";
    html +=
        "#console::-webkit-scrollbar-track{background:rgba(255,255,255,0.05);}";
    html += "#console::-webkit-scrollbar-thumb{background:rgba(102,126,234,0.5)"
            ";border-radius:4px;}";

    // Progress bar
    html += ".progress-container{width:100%;background:rgba(255,255,255,0.1);"
            "border-radius:8px;margin-top:15px;display:none;overflow:hidden;}";
    html += ".progress-bar{height:24px;background:linear-gradient(90deg,#"
            "667eea 0%,#764ba2 "
            "100%);border-radius:8px;text-align:center;line-height:24px;color:#"
            "fff;font-weight:600;transition:width 0.3s;box-shadow:0 0 10px "
            "rgba(102,126,234,0.5);}";

    // Link styling
    html += "a{color:#667eea;text-decoration:none;transition:color 0.3s;}";
    html += "a:hover{color:#764ba2;}";

    html += "</style>";

    // JavaScript
    html += "<script>";
    html += "function copyConsole(){";
    html += "  const c=document.getElementById('console');";
    html += "  const t=document.createElement('textarea');";
    html += "  t.value=c.innerText;document.body.appendChild(t);t.select();";
    html +=
        "  try{document.execCommand('copy');alert('✓ Copied to clipboard');}";
    html += "  catch(e){alert('✗ Failed to copy');}";
    html += "  document.body.removeChild(t);";
    html += "}";
    html += "function updateConsole(){";
    html += "  fetch('/console').then(r=>r.text()).then(t=>{";
    html += "    const c=document.getElementById('console');";
    html += "    if(c.innerText!=t){c.innerText=t;c.scrollTop=c.scrollHeight;}";
    html += "  });";
    html += "}";
    html += "function uploadFile(){";
    html += "  const file=document.getElementById('update-file').files[0];";
    html += "  if(!file)return;";
    html += "  const formData=new FormData();formData.append('update',file);";
    html += "  const xhr=new XMLHttpRequest();";
    html +=
        "  "
        "document.querySelector('.progress-container').style.display='block';";
    html += "  xhr.upload.addEventListener('progress',(e)=>{";
    html += "    if(e.lengthComputable){";
    html += "      const p=Math.round((e.loaded/e.total)*100);";
    html += "      const b=document.getElementById('up-bar');";
    html += "      b.style.width=p+'%';b.innerText=p+'%';";
    html += "    }";
    html += "  });";
    html += "  xhr.onreadystatechange=()=>{";
    html +=
        "    if(xhr.readyState==4){const msg=xhr.responseText;let c=5;const "
        "d=document.createElement('div');d.style='position:fixed;top:50%;left:"
        "50%;transform:translate(-50%,-50%);background:rgba(0,0,0,0.9);color:#"
        "fff;padding:30px;border-radius:12px;text-align:center;z-index:9999;"
        "font-size:1.2em;';d.innerHTML=msg+'<br><br>Reloading in <span "
        "id=\"cd\">'+c+'</span>s...';document.body.appendChild(d);const "
        "t=setInterval(()=>{c--;const "
        "e=document.getElementById('cd');if(e)e.innerText=c;if(c<=0){"
        "clearInterval(t);location.reload();}},1000);}";
    html += "  };";
    html += "  xhr.open('POST','/update',true);xhr.send(formData);";
    html += "}";
    html += "function updateUnits(){";
    html += "  fetch('/units').then(r=>r.json()).then(d=>{";
    html += "    const sel=document.querySelector('select[name=afcunit]');";
    html += "    if(!sel)return;";
    html += "    const curr=sel.value;";
    html += "    if(sel.options.length!=d.count){";
    html += "      sel.innerHTML='';";
    html += "      for(let i=0;i<d.count;i++){";
    html += "        const opt=document.createElement('option');";
    html += "        opt.value=i;opt.text='Unit '+i;";
    html += "        if(i==d.active)opt.selected=true;";
    html += "        sel.appendChild(opt);";
    html += "      }";
    html += "    }";
    html += "  }).catch(()=>{});";
    html += "}";
    html += "function updateStatus(){";
    html += "  fetch('/status').then(r=>r.json()).then(d=>{";
    html += "    const s=document.getElementById('printer-status');";
    html += "    const sd=document.getElementById('status-dot');";
    html += "    if(s)s.innerText=d.status;";
    html += "    if(sd)sd.style.background=d.online?'#4ade80':'#ff6b6b';";
    html += "  }).catch(()=>{});";
    html += "}";
    html += "function saveSettings(e){";
    html += "  e.preventDefault();const form=e.target;const data=new "
            "FormData(form);const params=new URLSearchParams(data);";
    html +=
        "  "
        "fetch('/save',{method:'POST',headers:{'Content-Type':'application/"
        "x-www-form-urlencoded'},body:params}).then(r=>r.text()).then(msg=>{";
    html +=
        "    let c=5;const "
        "d=document.createElement('div');d.style='position:fixed;top:50%;left:"
        "50%;transform:translate(-50%,-50%);background:rgba(0,0,0,0.9);color:#"
        "fff;padding:30px;border-radius:12px;text-align:center;z-index:9999;"
        "font-size:1.2em;';d.innerHTML=msg+'<br><br>Reloading in <span "
        "id=\"scd\">'+c+'</span>s...';document.body.appendChild(d);const "
        "t=setInterval(()=>{c--;const "
        "e=document.getElementById('scd');if(e)e.innerText=c;if(c<=0){"
        "clearInterval(t);location.reload();}},1000);";
    html += "  });";
    html += "}";
    html += "setInterval(updateConsole,2000);";
    html += "setInterval(updateUnits,3000);";
    html += "setInterval(updateStatus,2500);";
    html += "</script>";
    html += "</head><body>";

    // Header
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>🖥️ SC01+ Configuration</h1>";
    html += "<div class='badge'>v" FIRMWARE_VERSION "</div>";
    html += "\u003cdiv class='status'\u003e\u003cspan "
            "class='status-dot'\u003e\u003c/span\u003eDevice: " +
            String(WiFi.getHostname()) + "\u003c/div\u003e";
    html +=
        "\u003cdiv class='status' style='margin-left:20px;'\u003e\u003cspan "
        "id='status-dot' class='status-dot' style='background:" +
        String(_status == "Offline" ? "#ff6b6b" : "#4ade80") +
        ";'\u003e\u003c/span\u003ePrinter: \u003cspan "
        "id='printer-status'\u003e" +
        _status + "\u003c/span\u003e\u003c/div\u003e";
    html += "<p style='margin-top:10px;'><a href='/model' target='_blank'>📊 "
            "View Internal Object Model (JSON)</a></p>";
    html += "</div>";

    // Grid layout
    html += "<div class='grid'>";

    // Network Settings Card
    html += "<div class='card'>";
    html += "<div class='card-title'><span class='card-icon'>📡</span>Network "
            "Settings</div>";
    html += "<form action='/save' method='POST' "
            "onsubmit='saveSettings(event);return false;'>";
    html += "<label>WiFi SSID</label>";
    html += "<input type='text' name='ssid' value='" + _ssid + "' required>";
    html += "<label>WiFi Password</label>";
    html += "<input type='password' name='pass' value='" + _password + "'>";
    html += "<label>NTP Server</label>";
    html += "<input type='text' name='ntp' value='" + _ntpServer + "'>";

    // Timezone dropdown
    html += "<label>Timezone</label>";
    html += "<select name='timezone'>";
    struct {
      const char *label;
      int offset;
    } timezones[] = {{"UTC-12 (Baker Island)", -43200},
                     {"UTC-11 (American Samoa)", -39600},
                     {"UTC-10 (Hawaii)", -36000},
                     {"UTC-9 (Alaska)", -32400},
                     {"UTC-8 (PST - Los Angeles)", -28800},
                     {"UTC-7 (MST - Denver)", -25200},
                     {"UTC-6 (CST - Chicago)", -21600},
                     {"UTC-5 (EST - New York)", -18000},
                     {"UTC-4 (Atlantic)", -14400},
                     {"UTC-3 (Buenos Aires)", -10800},
                     {"UTC-2 (Mid-Atlantic)", -7200},
                     {"UTC-1 (Azores)", -3600},
                     {"UTC+0 (GMT - London)", 0},
                     {"UTC+1 (CET - Paris)", 3600},
                     {"UTC+2 (EET - Cairo)", 7200},
                     {"UTC+3 (Moscow)", 10800},
                     {"UTC+4 (Dubai)", 14400},
                     {"UTC+5 (Pakistan)", 18000},
                     {"UTC+5:30 (India)", 19800},
                     {"UTC+6 (Bangladesh)", 21600},
                     {"UTC+7 (Bangkok)", 25200},
                     {"UTC+8 (Singapore)", 28800},
                     {"UTC+9 (Tokyo)", 32400},
                     {"UTC+10 (Sydney)", 36000},
                     {"UTC+11 (Solomon Islands)", 39600},
                     {"UTC+12 (New Zealand)", 43200}};
    for (auto &tz : timezones) {
      html += "<option value='" + String(tz.offset) + "'";
      if (tz.offset == _gmtOffset)
        html += " selected";
      html += ">" + String(tz.label) + "</option>";
    }
    html += "</select>";
    html += "</div>";

    // Printer & AFC Settings Card
    html += "<div class='card'>";
    html += "<div class='card-title'><span class='card-icon'>🖨️</span>Printer "
            "& AFC Settings</div>";
    html +=
        "\u003clabel\u003ePrinter Address (IP or Hostname)\u003c/label\u003e";
    html += "\u003cinput type='text' name='rip' value='" + _printerIP +
            "' placeholder='printer.local or 192.168.1.100' required\u003e";
    html += "<label>Poll Rate (ms)</label>";
    html += "<input type='number' name='poll' value='" + String(_pollInterval) +
            "' min='100' max='10000'>";
    html += "<label>AFC Unit</label>";
    html += "<select name='afcunit'>";
    for (int i = 0; i < _unitCount; i++) {
      html += "<option value='" + String(i) + "'";
      if (i == _activeAFCUnit)
        html += " selected";
      html += ">Unit " + String(i) + "</option>";
    }
    html += "</select>";
    html += "<button type='submit' style='width:100%;margin-top:10px;'>💾 Save "
            "& Reconnect</button>";
    html += "</form>";
    html += "</div>";
    html += "</div>";

    // Firmware Update Card (full width)
    html += "<div class='card'>";
    html += "<div class='card-title'><span class='card-icon'>⬆️</span>Firmware "
            "Update</div>";
    html += "<input type='file' id='update-file' name='update' accept='.bin'>";
    html += "<button type='button' onclick='uploadFile()' "
            "style='margin-top:10px;'>🚀 Update Firmware</button>";
    html += "<div class='progress-container'><div id='up-bar' "
            "class='progress-bar'>0%</div></div>";
    html += "</div>";

    // System Console Card (full width)
    html += "<div class='card'>";
    html += "<div class='card-title'><span class='card-icon'>💻</span>System "
            "Console<button onclick='copyConsole()' class='btn-secondary "
            "btn-small' style='margin-left:auto;'>📋 Copy</button></div>";
    html += "<div id='console'>Loading logs...</div>";
    html += "</div>";

    html += "</div></div></body></html>";
    _server.send(200, "text/html", html);
  });

  _server.on("/model", HTTP_GET, [this]() {
    _server.send(200, "application/json", getModelJSON());
  });

  _server.on("/console", HTTP_GET, [this]() {
    String output = "=== SC01+ Firmware v" FIRMWARE_VERSION " ===\n\n";
    for (const auto &l : _logs) {
      output += l + "\n";
    }
    _server.send(200, "text/plain", output);
  });

  _server.on("/save", HTTP_POST, [this]() {
    if (_server.hasArg("ssid"))
      _ssid = _server.arg("ssid");
    if (_server.hasArg("pass"))
      _password = _server.arg("pass");
    if (_server.hasArg("rip"))
      _printerIP = _server.arg("rip");
    if (_server.hasArg("poll"))
      _pollInterval = _server.arg("poll").toInt();
    if (_server.hasArg("ntp"))
      _ntpServer = _server.arg("ntp");
    if (_server.hasArg("timezone"))
      _gmtOffset = _server.arg("timezone").toInt();
    if (_server.hasArg("afcunit"))
      setActiveAFCUnit(_server.arg("afcunit").toInt());

    saveSettings();
    log("Settings saved via Web UI. Reconnecting...");
    _server.send(200, "text/plain", "Settings saved. Reconnecting...");
    delay(1000);
    WiFi.begin(_ssid.c_str(), _password.c_str());
  });

  _server.on(
      "/update", HTTP_POST,
      [this]() {
        _server.sendHeader("Connection", "close");
        _server.send(200, "text/plain",
                     (Update.hasError()) ? "FAIL" : "OK. Rebooting...");
        delay(1000);
        ESP.restart();
      },
      [this]() {
        HTTPUpload &upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          _otaInProgress = true; // Pause background tasks during OTA
          log(("OTA Start: " + upload.filename).c_str());
          // Explicitly use U_FLASH and check begin()
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            log(("Update Begin Error: " + String(Update.errorString()))
                    .c_str());
            Update.printError(Serial);
            _otaInProgress = false; // Reset on error
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            log("Update Write Error");
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {
            log(("OTA Success: " + String(upload.totalSize) + " bytes")
                    .c_str());
          } else {
            log(("Update End Error: " + String(Update.errorString())).c_str());
            Update.printError(Serial);
          }
          _otaInProgress = false; // Reset after upload completes
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
          _otaInProgress = false; // Reset on abort
        }
      });

  // Units API endpoint for dynamic updates
  _server.on("/units", HTTP_GET, [this]() {
    String json = "{\"count\":" + String(_unitCount) +
                  ",\"active\":" + String(_activeAFCUnit) + "}";
    _server.send(200, "application/json", json);
  });

  _server.on("/status", HTTP_GET, [this]() {
    String json = "{\"status\":\"" + _status + "\",\"online\":" +
                  String(_status != "Offline" ? "true" : "false") + "}";
    _server.send(200, "application/json", json);
  });

  _server.begin();
  log("Web Server Started.");
}

void NetworkManager::handleWebServer() { _server.handleClient(); }

String NetworkManager::getModelJSON() {
  DynamicJsonDocument combined(32768); // Increased to 32KB for all models
  combined["heat"] = _modelHeat;
  combined["state"] = _modelState;
  combined["job"] = _modelJob;
  combined["network"] = _modelNetwork;
  combined["global"] = _modelGlobal;
  String output;
  serializeJson(combined, output);
  return output;
}

void NetworkManager::updatePrinterStatus() {
  if (_printerIP.length() == 0)
    return;

  // Sequence: state, job, network, then 5 global keys
  String key = "state";
  static const char *globalKeys[] = {
      "global.AFC_lanes", "global.AFC_LED_array", "global.AFC_lane_to_tool",
      "global.AFC_unit_total_lanes", "global.Tool_to_AFC"};

  if (_queryIndex == 1)
    key = "job";
  else if (_queryIndex == 2)
    key = "network";
  else if (_queryIndex >= 3 && _queryIndex <= 7) {
    int keyOffset = _queryIndex - 3;
    key = globalKeys[keyOffset];
  }

  _queryIndex = (_queryIndex + 1) % 8; // 3 core keys + 5 global keys

  // 1. Check if we have a network connection first
  if (WiFi.status() != WL_CONNECTED) {
    _status = "Offline";
    return;
  }

  // 2. Resolve hostname if needed (caching to avoid blocking DNS)
  String targetIP = _printerIP;
  IPAddress ip;

  // Check if _printerIP is already a valid IP string
  if (ip.fromString(_printerIP)) {
    targetIP = _printerIP; // It's an IP, use as is
  } else {
    // It's a hostname, check if we need to resolve it
    if (!_ipResolved || _cachedIP == IPAddress(0, 0, 0, 0)) {
      // Try to resolve
      IPAddress resolvedIP;
      if (WiFi.hostByName(_printerIP.c_str(), resolvedIP)) {
        _cachedIP = resolvedIP;
        _ipResolved = true;
        Serial.printf("NET: Resolved %s to %s\n", _printerIP.c_str(),
                      _cachedIP.toString().c_str());
      } else {
        // Resolve failed
        if (_cachedIP != IPAddress(0, 0, 0, 0)) {
          Serial.println("NET: DNS failed, using cached IP");
          // Proceed with cached IP, but keep _ipResolved false to try again
          // next time
        } else {
          _status = "Offline";
          return; // No IP to connect to
        }
      }
    }
    targetIP = _cachedIP.toString();
  }

  HTTPClient http;
  String url = "http://" + targetIP + "/rr_model?key=" + key;

  // Use shorter timeouts when offline to prevent UI blocking
  int timeout = (_status == "Offline") ? 200 : 500; // Reduced timeouts further
  http.setTimeout(timeout);
  http.setConnectTimeout(timeout); // Also set connection timeout
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    DynamicJsonDocument doc(24576); // Use 24KB for robust parsing
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // Successful response - clear offline status if it was set
      if (_status == "Offline") {
        _status = "Idle"; // Default to idle, will be updated from state
        log("Printer back online");
      }

      JsonObject root = doc.as<JsonObject>();
      String keyReceived = root["key"] | "";
      JsonVariant res = root["result"];

      if (keyReceived == "heat") {
        _modelHeat.clear();
        _modelHeat.set(res);
      } else if (keyReceived == "tools") {
        _modelTools.clear();
        _modelTools.set(res);
      } else if (keyReceived == "state") {
        _modelState.clear();
        _modelState.set(res);
      } else if (keyReceived == "job") {
        _modelJob.clear();
        _modelJob.set(res);
      } else if (keyReceived == "network") {
        _modelNetwork.clear();
        _modelNetwork.set(res);
      } else if (keyReceived.startsWith("global.")) {
        // Handle specific global variables selectively
        String subKey = keyReceived.substring(7); // Remove "global."
        _modelGlobal[subKey] = res;

        // Specialized handling for AFC_lanes to maintain legacy logic
        if (subKey == "AFC_lanes" && res.is<JsonArray>()) {
          JsonArray units = res.as<JsonArray>();
          for (int u = 0; u < units.size() && u < 8; u++) {
            JsonArray lanes = units[u].as<JsonArray>();
            for (int l = 0; l < lanes.size() && l < 4; l++) {
              int toolIdx = u * 4 + l;
              if (toolIdx < 32) {
                _laneLoaded[toolIdx] = lanes[l][0].as<bool>();
                if (lanes[l].size() > 4 && lanes[l][4].is<JsonArray>()) {
                  JsonArray info = lanes[l][4].as<JsonArray>();
                  if (info.size() > 0) {
                    _laneNames[toolIdx] = info[0].as<String>();
                  }
                }
              }
            }
          }
        }

        // Update unit count from AFC_unit_total_lanes
        if (subKey == "AFC_unit_total_lanes" && res.is<JsonArray>()) {
          JsonArray unitLanes = res.as<JsonArray>();
          _unitCount = unitLanes.size();
        }

        // Compact memory to prevent fragmentation
        _modelGlobal.garbageCollect();
      }
    } else {
      log(("PARSE ERR: " + String(error.c_str())).c_str());
      // Don't block recovery - continue polling
    }
  } else {
    // HTTP request failed
    if (httpCode <= 0) {
      // Connection error - printer likely offline
      if (_status != "Offline") {
        _status = "Offline";
        log("Printer offline - will retry");
      }
    } else {
      // HTTP error code (404, 500, etc.)
      log(("HTTP ERR: " + String(httpCode)).c_str());
    }
    // Continue polling to allow recovery
  }
  http.end();

  // Update members from the REPLICATED model branches
  JsonObject heat = _modelHeat.as<JsonObject>();
  JsonObject state = _modelState.as<JsonObject>();
  JsonObject job = _modelJob.as<JsonObject>();
  JsonObject network = _modelNetwork.as<JsonObject>();
  JsonArray toolsArr = _modelTools.as<JsonArray>();

  if (!heat.isNull()) {
    JsonArray heaters = heat["heaters"];
    if (heaters && heaters.size() > 0) {
      _bedTemp = heaters[0]["current"] | 0.0f;
      if (millis() - _lastCommandTime > _commandLockout) {
        _bedTarget = heaters[0]["active"] | 0.0f;
      }
    }

    if (!toolsArr.isNull()) {
      _toolCount = toolsArr.size();
      for (int i = 0; i < _toolCount && i < 10; i++) {
        JsonVariant tHeaters = toolsArr[i]["heaters"];
        if (tHeaters.is<JsonArray>()) {
          JsonArray hArr = tHeaters.as<JsonArray>();
          if (hArr.size() > 0) {
            int hIdx = hArr[0] | -1;
            if (hIdx >= 0 && heaters && heaters.size() > hIdx) {
              _toolTemps[i] = heaters[hIdx]["current"] | 0.0f;
              if (millis() - _lastCommandTime > _commandLockout) {
                _toolTargets[i] = heaters[hIdx]["active"] | 0.0f;
              }
            }
          }
        }
      }
    }
  }

  // Only update status from state if we're not offline
  // (don't let cached state overwrite offline status)
  if (!state.isNull() && _status != "Offline") {
    _status = state["status"].as<String>();
    if (_status.length() > 0)
      _status[0] = toupper(_status[0]);
  }

  // Heartbeat log: showing Status and Unit count
  static uint32_t lastLog = 0;
  if (millis() - lastLog > 5000) {
    String logMsg =
        "Machine: " + _status + " (Units: " + String(_unitCount) + ")";
    log(logMsg.c_str());
    lastLog = millis();
  }

  if (!network.isNull()) {
    _printerName = network["name"] | "PanelDue SC01+";
  }

  if (!job.isNull()) {
    float pos = job["filePosition"] | 0.0f;
    float size = job["file"]["size"] | 1.0f;
    if (size > 0)
      _progress = (pos / size) * 100.0f;
  }
}

void NetworkManager::sendGCode(const char *gcode) {
  if (!isConnected() || _printerIP.length() == 0)
    return;

  // URL Encode the G-code (minimally replace spaces with %20)
  String encoded = "";
  for (int i = 0; gcode[i] != '\0'; i++) {
    if (gcode[i] == ' ')
      encoded += "%20";
    else
      encoded += gcode[i];
  }

  HTTPClient http;
  String url = "http://" + _printerIP + "/rr_gcode?gcode=" + encoded;
  log(("GCODE SEND: " + url).c_str());
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("NET: GCode failed, HTTP %d\n", httpCode);
  }
  http.end();
}

void NetworkManager::setBedTarget(float temp) {
  _bedTarget = temp;
  if (_bedTarget < 0)
    _bedTarget = 0;
  _lastCommandTime = millis();
  char buf[64];
  // Set target and ensure bed is active (M144 S1)
  snprintf(buf, sizeof(buf), "M140 S%.0f", _bedTarget);
  sendGCode(buf);
  sendGCode("M144 S1");
}

void NetworkManager::setToolTarget(float temp) {
  _toolTargets[_selectedTool] = temp;
  if (_toolTargets[_selectedTool] < 0)
    _toolTargets[_selectedTool] = 0;
  _lastCommandTime = millis();
  char buf[64];
  // M568 sets active (S) and standby (R) temps. A2 sets Active state.
  snprintf(buf, sizeof(buf), "M568 P%d S%.0f A2", _selectedTool,
           _toolTargets[_selectedTool]);
  sendGCode(buf);
}

void NetworkManager::adjustBed(float delta) {
  setBedTarget(_bedTarget + delta);
}

void NetworkManager::adjustTool(float delta) {
  setToolTarget(_toolTargets[_selectedTool] + delta);
}

// Filament List Management
void NetworkManager::fetchFilamentList() {
  // Always fetch fresh data when requested to ensure latest filament list
  HTTPClient http;
  String url =
      "http://" + _printerIP + "/rr_download?name=0:/sys/filamentList.json";

  http.setTimeout(2000);
  http.setConnectTimeout(2000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc.containsKey("listValues")) {
      _filamentList.clear();
      JsonArray values = doc["listValues"].as<JsonArray>();
      for (JsonVariant v : values) {
        _filamentList.push_back(v.as<String>());
      }
      _lastFilamentFetch = millis();
      log("Filament list fetched successfully");
    } else {
      log("Failed to parse filament list JSON");
    }
  } else {
    log("Failed to fetch filament list");
  }

  http.end();
}

void NetworkManager::setLaneFilament(int unit, int lane, String filamentName) {
  // Update AFC_lanes[unit][lane][4][0] via G-code
  // Format: set global.AFC_lanes[unit][lane][4][0] = "filament_name"
  char buf[256];
  snprintf(buf, sizeof(buf), "set global.AFC_lanes[%d][%d][4][0] = \"%s\"",
           unit, lane, filamentName.c_str());
  sendGCode(buf);

  // Save the status to persist changes
  sendGCode("M98 P\"0:/sys/AFC/Macros/save_status.g\"");

  log(("Set filament for Unit " + String(unit) + " Lane " + String(lane) +
       ": " + filamentName)
          .c_str());
}

String NetworkManager::getLaneFilament(int unit, int lane) {
  // Get filament from AFC_lanes[unit][lane][4][0]
  if (_modelGlobal.containsKey("AFC_lanes")) {
    JsonArray units = _modelGlobal["AFC_lanes"].as<JsonArray>();
    if (unit >= 0 && unit < units.size()) {
      JsonArray lanes = units[unit].as<JsonArray>();
      if (lane >= 0 && lane < lanes.size()) {
        JsonArray laneData = lanes[lane].as<JsonArray>();
        if (laneData.size() > 4) {
          JsonArray filamentData = laneData[4].as<JsonArray>();
          if (filamentData.size() > 0) {
            String filament = filamentData[0].as<String>();
            return filament; // Return whatever is there, even if empty
          }
        }
      }
    }
  }
  return ""; // Return empty string if not found
}
