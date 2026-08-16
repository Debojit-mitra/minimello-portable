#include "server/web_server.h"
#include "config.h"
#include "logger.h"
#include "ota/ota_manager.h"
#include "version.h"
#include "web_ui_data.h"
#include <ArduinoJson.h>

extern OTAManager otaMgr;

void MiniWebServer::handleGetStatus(AsyncWebServerRequest *req) {
  JsonDocument doc;
  doc["version"] = FIRMWARE_VERSION;
  doc["name"] = FIRMWARE_NAME;
  doc["battery_percent"] = _power->getBatteryPercent();
  doc["battery_voltage"] = _power->getBatteryVoltage();
  doc["wifi_connected"] = _network->isConnected();
  doc["wifi_rssi"] = _network->getRSSI();
  doc["ip"] = _network->getIP();
  doc["uptime_s"] = millis() / 1000;
  doc["active_engine"] = (_screen->getActiveEngine() == ActiveEngine::EMOTION)
                             ? "emotion"
                             : "clock";
  doc["emotion"] = _screen->emotionEngine()->getEmotionName();
  doc["clock_face"] = _screen->clockEngine()->getFaceName();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["battery_enabled"] = (bool)ENABLE_BATTERY_MODULE;
  doc["has_update"] = otaMgr.isUpdateAvailable();
  doc["latest_version"] = otaMgr.getLatestVersion();

  String response;
  serializeJson(doc, response);

  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", response);
  addCorsHeaders(resp);
  req->send(resp);
}

void MiniWebServer::handleGetConfig(AsyncWebServerRequest *req) {
  JsonDocument doc;
  doc["wifi_ssid"] = _config->wifiSSID;
  doc["tz_offset"] = _config->tzOffset;
  doc["clock_face"] = _config->clockFace;
  doc["brightness"] = _config->brightness;
  doc["default_engine"] = _config->defaultEngine;
  doc["auto_switch"] = _config->autoSwitch;
  doc["switch_interval"] = _config->switchIntervalS;
  doc["clock_duration"] = _config->clockDurationS;
  doc["night_start"] = _config->nightStartHour;
  doc["night_end"] = _config->nightEndHour;
  doc["night_enabled"] = _config->nightModeEnabled;
  doc["weather_city"] = _config->weatherCity;
  doc["weather_lat"] = _config->weatherLat;
  doc["weather_lon"] = _config->weatherLon;
  doc["debug_mode"] = _config->debugMode;
  doc["mood_interval"] = _config->moodIntervalS;
  doc["oled_protection"] = _config->oledProtectionEnabled;
  doc["user_name"] = _config->userName;

  String response;
  serializeJson(doc, response);

  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", response);
  addCorsHeaders(resp);
  req->send(resp);
}

void MiniWebServer::handlePostConfig(AsyncWebServerRequest *req, uint8_t *data,
                                     size_t len) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  // Update only provided fields
  if (!doc["tz_offset"].isNull()) {
    _config->tzOffset = doc["tz_offset"].as<int>();
    _network->setTimezone(
        _config->tzOffset); // Apply live (configTime is thread-safe)
  }
  if (!doc["clock_face"].isNull())
    _config->clockFace = doc["clock_face"].as<int>();
  if (!doc["brightness"].isNull()) {
    _config->brightness = doc["brightness"].as<int>();
    _config->brightnessChanged = true;
  }
  if (!doc["default_engine"].isNull())
    _config->defaultEngine = doc["default_engine"].as<int>();
  if (!doc["auto_switch"].isNull())
    _config->autoSwitch = doc["auto_switch"].as<bool>();
  if (!doc["switch_interval"].isNull())
    _config->switchIntervalS = doc["switch_interval"].as<int>();
  if (!doc["clock_duration"].isNull())
    _config->clockDurationS = doc["clock_duration"].as<int>();
  if (!doc["night_start"].isNull())
    _config->nightStartHour = doc["night_start"].as<int>();
  if (!doc["night_end"].isNull())
    _config->nightEndHour = doc["night_end"].as<int>();
  if (!doc["night_enabled"].isNull())
    _config->nightModeEnabled = doc["night_enabled"].as<bool>();
  if (!doc["oled_protection"].isNull())
    _config->oledProtectionEnabled = doc["oled_protection"].as<bool>();
  if (!doc["weather_lat"].isNull()) {
    _config->weatherLat = doc["weather_lat"].as<float>();
    _config->weatherChanged = true;
  }
  if (!doc["weather_lon"].isNull()) {
    _config->weatherLon = doc["weather_lon"].as<float>();
    _config->weatherChanged = true;
  }
  if (doc["weather_city"].is<const char *>()) {
    _config->weatherCity = doc["weather_city"].as<String>();
    _config->weatherChanged = true;
  }
  if (doc["debug_mode"].is<bool>())
    _config->debugMode = doc["debug_mode"];
  if (!doc["mood_interval"].isNull())
    _config->moodIntervalS = doc["mood_interval"].as<int>();
  if (doc["user_name"].is<const char *>())
    _config->userName = doc["user_name"].as<String>();

  // Enforce OLED protection logic
  if (!_config->autoSwitch || _config->clockDurationS >= 60 || _config->switchIntervalS >= 60) {
      _config->oledProtectionEnabled = true;
  }

  _config->save();

  // Apply live changes (simple assignments — safe from async task on ESP32)
  _screen->setAutoSwitch(_config->autoSwitch);
  _screen->setAutoSwitchInterval(_config->switchIntervalS);
  _screen->setClockDuration(_config->clockDurationS);
  _screen->clockEngine()->setFace((ClockFace)_config->clockFace);
  _screen->emotionEngine()->setMoodInterval(_config->moodIntervalS);

  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", "{\"ok\":true}");
  addCorsHeaders(resp);
  req->send(resp);
}

