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
      _firstCycleComplete = false; // Reset fast-poll flag on reconnect
      _queriesSinceConnect = 0;
      log(("NTP Sync Started: " + _ntpServer).c_str());
    }
    handleWebServer();

    // Skip all other background operations during OTA upload
    if (_otaInProgress) {
      return;
    }

    // Fast-poll on first connect until we've cycled through all 8 queries once
    uint32_t pollInterval = _pollInterval;
    if (!_firstCycleComplete) {
      pollInterval = 500; // 500ms between queries until initial data loaded
    } else if (_status == "Offline") {
      pollInterval = 5000; // Slower when printer is offline
    }

    if (millis() - _lastUpdate > pollInterval) {
      updatePrinterStatus();
      _lastUpdate = millis();
      if (!_firstCycleComplete) {
        _queriesSinceConnect++;
        if (_queriesSinceConnect >= 8) _firstCycleComplete = true;
      }
    }
  }
}

void NetworkManager::loadSettings() {
  _ssid = _prefs.getString("ssid", "");
  _password = _prefs.getString("pass", "");
  _printerIP = _prefs.getString("rip", "");
  _ipResolved = false; // Force re-resolution
  _pollInterval = _prefs.getUInt("poll", 500);
  _ntpServer = _prefs.getString("ntp", "pool.ntp.org");
  _gmtOffset = _prefs.getLong("gmto", 0);
  _selectedTool = _prefs.getInt("tlidx", 0);
  _activeAFCUnit = _prefs.getInt("afcunit", 0);
  _theme = _prefs.getInt("theme", 0);
  _ssTimeout = _prefs.getInt("sstout", 30);
  _ssDimLevel = _prefs.getInt("ssdim", 20);
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
  _prefs.putInt("theme", _theme);
  _prefs.putInt("sstout", _ssTimeout);
  _prefs.putInt("ssdim", _ssDimLevel);
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
  // Use a short timeout so we never block the main loop waiting for NTP
  if (!getLocalTime(&timeinfo, 50)) {
    return String(millis() / 1000) + "s";
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

void NetworkManager::beginWebServer() {
  _server.on("/", HTTP_GET, [this]() {
    // Pre-reserve to avoid realloc churn; heap has ~177KB free so 20KB is fine
    String html;
    html.reserve(20000);

    auto s = [&html](const String &chunk) { html += chunk; };

    s("<html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>");
    s("<title>SC01+ Config v" FIRMWARE_VERSION "</title>");

    // CSS
    s("<style>");
    s("*{margin:0;padding:0;box-sizing:border-box;}");

    // Theme colours
    if (_theme == 1) {
      s(":root{--bg1:#f3f4f6;--bg2:#e5e7eb;--text:#1f2937;--text-muted:#4b5563;"
        "--card-bg:rgba(255,255,255,0.7);--card-border:rgba(0,0,0,0.1);"
        "--primary1:#3b82f6;--primary2:#2563eb;--accent:#10b981;"
        "--input-bg:rgba(0,0,0,0.05);}");
    } else if (_theme == 2) {
      s(":root{--bg1:#064e3b;--bg2:#022c22;--text:#d1fae5;--text-muted:#a7f3d0;"
        "--card-bg:rgba(6,78,59,0.5);--card-border:rgba(52,211,153,0.2);"
        "--primary1:#10b981;--primary2:#059669;--accent:#34d399;"
        "--input-bg:rgba(0,0,0,0.2);}");
    } else if (_theme == 3) {
      s(":root{--bg1:#2a0a2a;--bg2:#000022;--text:#00ffcc;--text-muted:#ff00ff;"
        "--card-bg:rgba(20,0,30,0.8);--card-border:rgba(255,0,255,0.4);"
        "--primary1:#ff00ff;--primary2:#cc00cc;--accent:#00ffcc;"
        "--input-bg:rgba(0,255,204,0.1);}");
    } else {
      s(":root{--bg1:#0f0f1e;--bg2:#1a1a2e;--text:#fff;--text-muted:#a0aec0;"
        "--card-bg:rgba(255,255,255,0.05);--card-border:rgba(255,255,255,0.1);"
        "--primary1:#667eea;--primary2:#764ba2;--accent:#4ade80;"
        "--input-bg:rgba(255,255,255,0.08);}");
    }

    s("body{font-family:'Segoe UI',Tahoma,sans-serif;"
      "background:linear-gradient(135deg,var(--bg1) 0%,var(--bg2) 100%);"
      "color:var(--text);padding:20px;min-height:100vh;}");
    s(".container{max-width:1200px;margin:0 auto;}");
    s(".header{text-align:center;margin-bottom:40px;}");
    s(".header h1{font-size:2.5em;margin-bottom:10px;"
      "background:linear-gradient(135deg,var(--primary1) 0%,var(--primary2) 100%);"
      "-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;}");
    s(".badge{display:inline-block;background:var(--input-bg);"
      "border:1px solid var(--card-border);padding:5px 15px;"
      "border-radius:20px;font-size:0.9em;margin-top:10px;}");
    s(".status{display:inline-block;margin-left:10px;}");
    s(".status-dot{display:inline-block;width:8px;height:8px;"
      "background:var(--accent);border-radius:50%;margin-right:5px;"
      "animation:pulse 2s infinite;}");
    s("@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.5;}}");
    s(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(500px,1fr));"
      "gap:25px;margin-bottom:25px;}");
    s("@media(max-width:768px){.grid{grid-template-columns:1fr;}}");
    s(".card{background:var(--card-bg);backdrop-filter:blur(10px);"
      "border:1px solid var(--card-border);border-radius:16px;padding:25px;"
      "box-shadow:0 8px 32px rgba(0,0,0,0.3);transition:transform 0.3s,box-shadow 0.3s;}");
    s(".card:hover{transform:translateY(-5px);box-shadow:0 12px 40px var(--card-border);}");
    s(".card-title{font-size:1.3em;margin-bottom:25px;display:flex;"
      "align-items:center;gap:10px;color:var(--primary1);}");
    s(".card-icon{font-size:1.5em;}");
    s("label{display:block;margin-bottom:8px;font-size:0.9em;color:var(--text-muted);}");
    s("input,select{width:100%;padding:12px;margin-bottom:15px;"
      "background:var(--input-bg);border:1px solid var(--card-border);"
      "border-radius:8px;color:var(--text);font-size:1em;transition:all 0.3s;}");
    s("input:focus,select:focus{outline:none;border-color:var(--primary1);"
      "background:var(--card-bg);box-shadow:0 0 0 3px var(--card-border);}");
    s("input[type='file']{padding:10px;cursor:pointer;}");
    s("option{background:var(--bg2);color:var(--text);}");
    s("button{padding:12px 24px;"
      "background:linear-gradient(135deg,var(--primary1) 0%,var(--primary2) 100%);"
      "color:#fff;border:none;border-radius:8px;cursor:pointer;"
      "font-size:1em;font-weight:600;transition:all 0.3s;"
      "box-shadow:0 4px 15px rgba(0,0,0,0.2);}");
    s("button:hover{transform:translateY(-2px);box-shadow:0 6px 20px rgba(0,0,0,0.3);}");
    s("button:active{transform:translateY(0);}");
    s(".btn-secondary{background:var(--input-bg);box-shadow:none;}");
    s(".btn-secondary:hover{background:var(--card-border);}");
    s(".btn-small{padding:6px 12px;font-size:0.85em;}");
    s("#console{background:rgba(0,0,0,0.5);color:var(--accent);padding:15px;"
      "height:250px;overflow-y:auto;font-family:'Courier New',monospace;"
      "font-size:0.9em;border-radius:8px;border:1px solid var(--card-border);line-height:1.6;}");
    s("#console::-webkit-scrollbar{width:8px;}");
    s("#console::-webkit-scrollbar-track{background:var(--input-bg);}");
    s("#console::-webkit-scrollbar-thumb{background:var(--primary1);border-radius:4px;}");
    s(".progress-container{width:100%;background:var(--input-bg);"
      "border-radius:8px;margin-top:15px;display:none;overflow:hidden;}");
    s(".progress-bar{height:24px;"
      "background:linear-gradient(90deg,var(--primary1) 0%,var(--primary2) 100%);"
      "border-radius:8px;text-align:center;line-height:24px;color:#fff;"
      "font-weight:600;transition:width 0.3s;box-shadow:0 0 10px rgba(0,0,0,0.5);}");
    s("a{color:var(--primary1);text-decoration:none;transition:color 0.3s;}");
    s("a:hover{color:var(--primary2);}");
    s("</style>");

    // JavaScript
    s("<script>");
    s("function copyConsole(){"
      "const c=document.getElementById('console');"
      "const t=document.createElement('textarea');"
      "t.value=c.innerText;document.body.appendChild(t);t.select();"
      "try{document.execCommand('copy');alert('\\u2713 Copied to clipboard');}"
      "catch(e){alert('\\u2717 Failed to copy');}"
      "document.body.removeChild(t);}");
    s("function updateConsole(){"
      "fetch('/console').then(r=>r.text()).then(t=>{"
      "const c=document.getElementById('console');"
      "if(c.innerText!=t){c.innerText=t;c.scrollTop=c.scrollHeight;}"
      "});}");
    s("function uploadFile(){"
      "const file=document.getElementById('update-file').files[0];"
      "if(!file)return;"
      "const formData=new FormData();formData.append('update',file);"
      "const xhr=new XMLHttpRequest();"
      "document.querySelector('.progress-container').style.display='block';"
      "xhr.upload.addEventListener('progress',(e)=>{"
      "if(e.lengthComputable){"
      "const p=Math.round((e.loaded/e.total)*100);"
      "const b=document.getElementById('up-bar');"
      "b.style.width=p+'%';b.innerText=p+'%';}});"
      "xhr.onreadystatechange=()=>{"
      "if(xhr.readyState==4){const msg=xhr.responseText;let c=5;"
      "const d=document.createElement('div');"
      "d.style='position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);"
      "background:rgba(0,0,0,0.9);color:#fff;padding:30px;border-radius:12px;"
      "text-align:center;z-index:9999;font-size:1.2em;';"
      "d.innerHTML=msg+'<br><br>Reloading in <span id=\"cd\">'+c+'</span>s...';"
      "document.body.appendChild(d);"
      "const t=setInterval(()=>{c--;const e=document.getElementById('cd');"
      "if(e)e.innerText=c;if(c<=0){clearInterval(t);location.reload();}},1000);}};"
      "xhr.open('POST','/update',true);xhr.send(formData);}");
    s("function updateUnits(){"
      "fetch('/units').then(r=>r.json()).then(d=>{"
      "const sel=document.querySelector('select[name=afcunit]');"
      "if(!sel)return;"
      "if(sel.options.length!=d.count){"
      "sel.innerHTML='';"
      "for(let i=0;i<d.count;i++){"
      "const opt=document.createElement('option');"
      "opt.value=i;opt.text='Unit '+i;"
      "if(i==d.active)opt.selected=true;"
      "sel.appendChild(opt);}}"
      "}).catch(()=>{});}");
    s("function updateStatus(){"
      "fetch('/status').then(r=>r.json()).then(d=>{"
      "const st=document.getElementById('printer-status');"
      "const sd=document.getElementById('status-dot');"
      "if(st)st.innerText=d.status;"
      "if(sd)sd.style.background=d.online?'#4ade80':'#ff6b6b';"
      "}).catch(()=>{});}");
    s("function saveSettings(e){"
      "e.preventDefault();const form=e.target;"
      "const data=new FormData(form);const params=new URLSearchParams(data);"
      "fetch('/save',{method:'POST',"
      "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
      "body:params}).then(r=>r.text()).then(msg=>{"
      "let c=5;const d=document.createElement('div');"
      "d.style='position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);"
      "background:rgba(0,0,0,0.9);color:#fff;padding:30px;border-radius:12px;"
      "text-align:center;z-index:9999;font-size:1.2em;';"
      "d.innerHTML=msg+'<br><br>Reloading in <span id=\"scd\">'+c+'</span>s...';"
      "document.body.appendChild(d);"
      "const t=setInterval(()=>{c--;const e=document.getElementById('scd');"
      "if(e)e.innerText=c;if(c<=0){clearInterval(t);location.reload();}},1000);"
      "});}"
      "setInterval(updateConsole,2000);"
      "setInterval(updateUnits,3000);"
      "setInterval(updateStatus,2500);");
    s("</script></head><body>");

    // Header
    s("<div class='container'><div class='header'>");
    s("<h1>&#x1F5A5;&#xFE0F; SC01+ Configuration</h1>");
    s("<div class='badge'>v" FIRMWARE_VERSION "</div>");
    s("<div class='status'><span class='status-dot'></span>Device: " +
      String(WiFi.getHostname()) + "</div>");
    s("<div class='status' style='margin-left:20px;'>"
      "<span id='status-dot' class='status-dot' style='background:" +
      String(_status == "Offline" ? "#ff6b6b" : "#4ade80") +
      ";'></span>Printer: <span id='printer-status'>" + _status + "</span></div>");
    s("<p style='margin-top:10px;'><a href='/model' target='_blank'>"
      "&#x1F4CA; View Internal Object Model (JSON)</a></p>");
    s("</div>");

    // Grid
    s("<div class='grid'>");

    // ── Network Settings Card ──
    s("<div class='card'>");
    s("<div class='card-title'><span class='card-icon'>&#x1F4E1;</span>Network Settings</div>");
    s("<form action='/save' method='POST' onsubmit='saveSettings(event);return false;'>");
    s("<label>WiFi SSID</label>");
    s("<input type='text' name='ssid' value='" + _ssid + "' required>");
    s("<label>WiFi Password</label>");
    s("<input type='password' name='pass' value='" + _password + "'>");
    s("<label>NTP Server</label>");
    s("<input type='text' name='ntp' value='" + _ntpServer + "'>");
    s("<label>Timezone</label><select name='timezone'>");

    struct { const char *label; int offset; } timezones[] = {
      {"UTC-12 (Baker Island)", -43200}, {"UTC-11 (American Samoa)", -39600},
      {"UTC-10 (Hawaii)", -36000},       {"UTC-9 (Alaska)", -32400},
      {"UTC-8 (PST - Los Angeles)", -28800}, {"UTC-7 (MST - Denver)", -25200},
      {"UTC-6 (CST - Chicago)", -21600}, {"UTC-5 (EST - New York)", -18000},
      {"UTC-4 (Atlantic)", -14400},      {"UTC-3 (Buenos Aires)", -10800},
      {"UTC-2 (Mid-Atlantic)", -7200},   {"UTC-1 (Azores)", -3600},
      {"UTC+0 (GMT - London)", 0},       {"UTC+1 (CET - Paris)", 3600},
      {"UTC+2 (EET - Cairo)", 7200},     {"UTC+3 (Moscow)", 10800},
      {"UTC+4 (Dubai)", 14400},          {"UTC+5 (Pakistan)", 18000},
      {"UTC+5:30 (India)", 19800},       {"UTC+6 (Bangladesh)", 21600},
      {"UTC+7 (Bangkok)", 25200},        {"UTC+8 (Singapore)", 28800},
      {"UTC+9 (Tokyo)", 32400},          {"UTC+10 (Sydney)", 36000},
      {"UTC+11 (Solomon Islands)", 39600}, {"UTC+12 (New Zealand)", 43200}
    };
    for (auto &tz : timezones) {
      String opt = "<option value='" + String(tz.offset) + "'";
      if (tz.offset == _gmtOffset) opt += " selected";
      opt += ">" + String(tz.label) + "</option>";
      s(opt);
    }
    s("</select></div>"); // close timezone select + Network card div (form still open)

    // ── Printer & AFC Settings Card ──
    s("<div class='card'>");
    s("<div class='card-title'><span class='card-icon'>&#x1F5A8;&#xFE0F;</span>Printer &amp; AFC Settings</div>");
    s("<label>Printer Address (IP or Hostname)</label>");
    s("<input type='text' name='rip' value='" + _printerIP +
      "' placeholder='printer.local or 192.168.1.100' required>");
    s("<label>Poll Rate (ms)</label>");
    s("<input type='number' name='poll' value='" + String(_pollInterval) +
      "' min='100' max='10000'>");
    s("<label>AFC Unit</label><select name='afcunit'>");
    for (int i = 0; i < _unitCount; i++) {
      String opt = "<option value='" + String(i) + "'";
      if (i == _activeAFCUnit) opt += " selected";
      opt += ">Unit " + String(i) + "</option>";
      s(opt);
    }
    s("</select>");
    s("<label>Screensaver Timeout (seconds, 0 = off)</label>");
    s("<input type='number' name='sstout' value='" + String(_ssTimeout) + "' min='0' max='3600'>");
    s("<label>Screensaver Dim Level (0-255, default 20)</label>");
    s("<input type='number' name='ssdim' value='" + String(_ssDimLevel) + "' min='0' max='255'>");
    s("<label>Web UI &amp; Display Theme</label><select name='theme'>");
    const char *themes[] = {"Dark Purple (Default)", "Light", "Dark Green", "Cyberpunk"};
    for (int i = 0; i < 4; i++) {
      String opt = "<option value='" + String(i) + "'";
      if (i == _theme) opt += " selected";
      opt += ">" + String(themes[i]) + "</option>";
      s(opt);
    }
    s("</select>");
    s("<button type='submit' style='width:100%;margin-top:10px;'>&#x1F4BE; Save &amp; Reconnect</button>");
    s("</form></div>"); // close form + Printer card
    s("</div>"); // close grid

    // ── Firmware Update Card ──
    s("<div class='card'>");
    s("<div class='card-title'><span class='card-icon'>&#x2B06;&#xFE0F;</span>Firmware Update</div>");
    s("<input type='file' id='update-file' name='update' accept='.bin'>");
    s("<button type='button' onclick='uploadFile()' style='margin-top:10px;'>&#x1F680; Update Firmware</button>");
    s("<div class='progress-container'><div id='up-bar' class='progress-bar'>0%</div></div>");
    s("</div>");

    // ── System Console Card ──
    s("<div class='card'>");
    s("<div class='card-title'><span class='card-icon'>&#x1F4BB;</span>System Console"
      "<button onclick='copyConsole()' class='btn-secondary btn-small' style='margin-left:auto;'>"
      "&#x1F4CB; Copy</button></div>");
    s("<div id='console'>Loading logs...</div>");
    s("</div>");

    s("</div></div></body></html>");
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
    if (_server.hasArg("theme"))
      setTheme(_server.arg("theme").toInt());
    if (_server.hasArg("sstout")) {
      int t = _server.arg("sstout").toInt();
      if (t < 0) t = 0;
      _ssTimeout = t;
    }
    if (_server.hasArg("ssdim")) {
      int d = _server.arg("ssdim").toInt();
      if (d < 0) d = 0;
      if (d > 255) d = 255;
      _ssDimLevel = d;
    }

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
          _otaInProgress = true;
          WiFi.setSleep(false);
          log(("OTA Start: " + upload.filename).c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            log(("Update Begin Error: " + String(Update.errorString())).c_str());
            Update.printError(Serial);
            _otaInProgress = false;
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            log("Update Write Error");
            Update.printError(Serial);
          }
          yield();
          delay(1);
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {
            log(("OTA Success: " + String(upload.totalSize) + " bytes").c_str());
          } else {
            log(("Update End Error: " + String(Update.errorString())).c_str());
            Update.printError(Serial);
          }
          _otaInProgress = false;
          WiFi.setSleep(true);
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
          _otaInProgress = false;
          WiFi.setSleep(true);
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
  DynamicJsonDocument combined(32768);
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

  if (ip.fromString(_printerIP)) {
    targetIP = _printerIP; // It's an IP, use as is
  } else {
    // It's a hostname — resolve once and cache
    if (!_ipResolved || _cachedIP == IPAddress(0, 0, 0, 0)) {
      // Backoff: don't call hostByName() if it failed recently (blocks ~8s)
      if (_dnsFailedMs != 0 && (millis() - _dnsFailedMs) < 30000UL) {
        _status = "Offline";
        return; // Still in backoff window — skip blocking DNS call
      }
      IPAddress resolvedIP;
      if (WiFi.hostByName(_printerIP.c_str(), resolvedIP)) {
        _cachedIP = resolvedIP;
        _ipResolved = true;
        _dnsFailedMs = 0; // Clear backoff on success
        Serial.printf("NET: Resolved %s to %s\n", _printerIP.c_str(),
                      _cachedIP.toString().c_str());
      } else {
        _dnsFailedMs = millis(); // Start 30s backoff
        if (_cachedIP != IPAddress(0, 0, 0, 0)) {
          Serial.println("NET: DNS failed, using cached IP");
        } else {
          _status = "Offline";
          return;
        }
      }
    }
    targetIP = _cachedIP.toString();
  }

  HTTPClient http;
  String url = "http://" + targetIP + "/rr_model?key=" + key;

  int timeout = (_status == "Offline") ? 200 : 500;
  http.setTimeout(timeout);
  http.setConnectTimeout(timeout);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      if (_status == "Offline") {
        _status = "Idle";
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
        String subKey = keyReceived.substring(7);
        _modelGlobal[subKey] = res;

        if (subKey == "AFC_lanes" && res.is<JsonArray>()) {
          JsonArray units = res.as<JsonArray>();
          for (int u = 0; u < (int)units.size() && u < 8; u++) {
            JsonArray lanes = units[u].as<JsonArray>();
            int actLanes = lanes.size();
            for (int l = 0; l < actLanes; l++) {
              int toolIdx = u * 16 + l;
              if (toolIdx < 128) {
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

        if (subKey == "AFC_unit_total_lanes" && res.is<JsonArray>()) {
          JsonArray unitLanes = res.as<JsonArray>();
          _unitCount = unitLanes.size();
          for (int i = 0; i < _unitCount && i < 8; i++) {
            _lanesPerUnit[i] = unitLanes[i] | 4;
          }
        }

        _modelGlobal.garbageCollect();
      }
    } else {
      log(("PARSE ERR: " + String(error.c_str())).c_str());
    }
  } else {
    if (httpCode <= 0) {
      if (_status != "Offline") {
        _status = "Offline";
        log("Printer offline - will retry");
      }
    } else {
      log(("HTTP ERR: " + String(httpCode)).c_str());
    }
  }
  http.end();

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
            if (hIdx >= 0 && heaters && (int)heaters.size() > hIdx) {
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

  if (!state.isNull() && _status != "Offline") {
    _status = state["status"].as<String>();
    if (_status.length() > 0)
      _status[0] = toupper(_status[0]);
  }

  static uint32_t lastLog = 0;
  if (millis() - lastLog > 5000) {
    String logMsg = "Machine: " + _status + " (Units: " + String(_unitCount) + ")";
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
  char buf[256];
  snprintf(buf, sizeof(buf), "set global.AFC_lanes[%d][%d][4][0] = \"%s\"",
           unit, lane, filamentName.c_str());
  sendGCode(buf);
  sendGCode("M98 P\"0:/sys/AFC/Macros/save_status.g\"");
  log(("Set filament for Unit " + String(unit) + " Lane " + String(lane) +
       ": " + filamentName).c_str());
}

String NetworkManager::getLaneFilament(int unit, int lane) {
  if (_modelGlobal.containsKey("AFC_lanes")) {
    JsonArray units = _modelGlobal["AFC_lanes"].as<JsonArray>();
    if (unit >= 0 && unit < (int)units.size()) {
      JsonArray lanes = units[unit].as<JsonArray>();
      if (lane >= 0 && lane < (int)lanes.size()) {
        JsonArray laneData = lanes[lane].as<JsonArray>();
        if (laneData.size() > 4) {
          JsonArray filamentData = laneData[4].as<JsonArray>();
          if (filamentData.size() > 0) {
            return filamentData[0].as<String>();
          }
        }
      }
    }
  }
  return "";
}
