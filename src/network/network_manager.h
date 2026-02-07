#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <deque>

#define FIRMWARE_VERSION "1.0.0"

class NetworkManager {
public:
  void init();
  void loop();
  void connectWiFi(const char *ssid, const char *password);
  void scanNetworks();
  bool isConnected();
  String getIP();
  String getSSID() { return _ssid; }
  String getPass() { return _password; }
  String getStatus() { return _status; }
  String getPrinterName() { return _printerName; }
  String getFormattedTime();

  // Web Server & OTA
  void beginWebServer();
  void handleWebServer();
  void log(const char *msg);
  void sendGCode(const char *gcode);
  void setBedTarget(float temp);
  void setToolTarget(float temp);
  void adjustBed(float delta);
  void adjustTool(float delta);

  // Printer Data
  void setPrinterIP(const char *ip);
  String getPrinterIP() { return _printerIP; }
  void updatePrinterStatus();
  float getBedTemp() { return _bedTemp; }
  float getBedTarget() { return _bedTarget; }
  float getToolTemp() { return _toolTemps[_selectedTool]; }
  float getToolTarget() { return _toolTargets[_selectedTool]; }
  int getSelectedTool() { return _selectedTool; }
  void setSelectedTool(int idx) {
    Serial.printf("NET: Tool change to %d\n", idx);
    _selectedTool = idx;
    saveSettings();
  }
  int getToolCount() { return _toolCount; }
  float getProgress() { return _progress; }
  uint32_t getPollInterval() { return _pollInterval; }
  void setPollInterval(uint32_t ms) {
    _pollInterval = ms;
    saveSettings();
  }

  int getActiveAFCUnit() { return _activeAFCUnit; }
  void setActiveAFCUnit(int unit) {
    _activeAFCUnit = unit;
    saveSettings();
  }
  int getUnitCount() { return _unitCount; }

  bool isLaneLoaded(int idx) {
    if (idx >= 0 && idx < 32)
      return _laneLoaded[idx];
    return false;
  }

  String getLaneName(int idx) {
    if (idx >= 0 && idx < 32)
      return _laneNames[idx];
    return "";
  }
  int getLaneToTool(int unit, int lane) {
    // Access AFC_lane_to_tool[unit][lane] from the global model
    if (_modelGlobal.containsKey("AFC_lane_to_tool")) {
      JsonArray units = _modelGlobal["AFC_lane_to_tool"].as<JsonArray>();
      if (unit >= 0 && unit < units.size()) {
        JsonArray lanes = units[unit].as<JsonArray>();
        if (lane >= 0 && lane < lanes.size()) {
          return lanes[lane].as<int>();
        }
      }
    }
    // Fallback to calculated tool index if data not available
    return unit * 4 + lane;
  }

  int getLEDColor(int unit, int lane) {
    // Access AFC_LED_array[unit][lane] from the global model
    // Returns: 0=red, 1=green, 2=blue, 3=white, 4=yellow, 5=magenta, 6=cyan
    if (_modelGlobal.containsKey("AFC_LED_array")) {
      JsonArray units = _modelGlobal["AFC_LED_array"].as<JsonArray>();
      if (unit >= 0 && unit < units.size()) {
        JsonArray lanes = units[unit].as<JsonArray>();
        if (lane >= 0 && lane < lanes.size()) {
          return lanes[lane].as<int>();
        }
      }
    }
    return -1; // No LED data available
  }

  // Filament List Management
  void fetchFilamentList();
  int getFilamentCount() { return _filamentList.size(); }
  String getFilamentName(int idx) {
    if (idx >= 0 && idx < _filamentList.size())
      return _filamentList[idx];
    return "";
  }
  void setLaneFilament(int unit, int lane, String filamentName);
  String getLaneFilament(int unit, int lane);

  String getModelJSON(); // Returns serialized current model

private:
  void loadSettings();
  void saveSettings();

  DynamicJsonDocument _modelHeat{8192};
  DynamicJsonDocument _modelGlobal{8192}; // Increased for AFC arrays
  DynamicJsonDocument _modelState{4096};
  DynamicJsonDocument _modelJob{4096};
  DynamicJsonDocument _modelNetwork{2048};
  DynamicJsonDocument _modelTools{8192};
  String _ssid;
  String _password;
  String _printerIP;

  String _status = "Disconnected";
  String _printerName = "PanelDue SC01+ v" FIRMWARE_VERSION;
  float _bedTemp = 0;
  float _bedTarget = 0;
  float _toolTemps[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  float _toolTargets[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int _toolCount = 1;
  int _selectedTool = 0;
  float _progress = 0;
  uint32_t _pollInterval = 1500;
  uint32_t _lastUpdate = 0;
  uint8_t _queryIndex = 0;

  int _activeAFCUnit = 0;
  int _unitCount = 1;
  bool _laneLoaded[32] = {false};
  String _laneNames[32];

  uint32_t _lastCommandTime = 0;
  uint32_t _commandLockout = 3000; // 3 seconds lockout on UI updates

  String _ntpServer = "pool.ntp.org";
  long _gmtOffset = 0;
  bool _ntpStarted = false;
  bool _otaInProgress = false; // Track OTA upload to pause background tasks

  std::deque<String> _logs;
  WebServer _server{80};
  Preferences _prefs;

  // Filament list
  std::vector<String> _filamentList;
  uint32_t _lastFilamentFetch = 0;

  // content
  IPAddress _cachedIP;
  bool _ipResolved = false;
};

extern NetworkManager DataManager;
