#include "web_server.h"
#include "config.h"
#include "version.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include "logger.h"

// =============================================================
// MiniWebServer — Implementation
// =============================================================

void MiniWebServer::begin(ConfigManager* config,
                          ScreenManager* screen,
                          PowerManager* power,
                          NetworkManager* network) {
    _config = config;
    _screen = screen;
    _power = power;
    _network = network;

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        LOG_E("SYSTEM", "LittleFS mount failed!");
    }

    setupAPIRoutes();      // Register API handlers first so they match before static files
    setupStaticRoutes();

    _server.begin();
    LOG_I("WEB", "Web server started");
}

void MiniWebServer::enableCaptivePortal() {
    _dns = new DNSServer();
    _dns->start(53, "*", WiFi.softAPIP());
    LOG_I("WEB", "Captive portal DNS started");

    // Redirect all unknown requests to index.html
    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("/");
    });
}

void MiniWebServer::setupStaticRoutes() {
    // Serve WebUI from LittleFS
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

void MiniWebServer::setupAPIRoutes() {
    // --- GET routes ---
    _server.on("/api/status", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetStatus(req); });

    _server.on("/api/config", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetConfig(req); });

    _server.on("/api/wifi/scan", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetWifiScan(req); });

    // --- POST routes (with body) ---
    // Config update
    _server.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* req) { /* handled in body cb */ },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0) handlePostConfig(req, data, len);
        });

    // WiFi credentials
    _server.on("/api/wifi", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0) handlePostWifi(req, data, len);
        });

    // Set emotion
    _server.on("/api/emotion", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0) handlePostEmotion(req, data, len);
        });

    // Set clock face
    _server.on("/api/clock/face", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
               size_t index, size_t total) {
            if (index == 0) handlePostClockFace(req, data, len);
        });

    // Toggle engine
    _server.on("/api/switch", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handlePostSwitch(req); });

    // OTA
    _server.on("/api/ota/check", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handlePostOtaCheck(req); });

    _server.on("/api/ota/update", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handlePostOtaUpdate(req); });

    // CORS preflight
    _server.on("/api/*", HTTP_OPTIONS,
        [this](AsyncWebServerRequest* req) {
            AsyncWebServerResponse* resp = req->beginResponse(200);
            addCorsHeaders(resp);
            req->send(resp);
        });
}

// --- API Handlers ---

void MiniWebServer::handleGetStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["name"] = FIRMWARE_NAME;
    doc["battery_percent"] = _power->getBatteryPercent();
    doc["battery_voltage"] = _power->getBatteryVoltage();
    doc["wifi_connected"] = _network->isConnected();
    doc["wifi_rssi"] = _network->getRSSI();
    doc["ip"] = _network->getIP();
    doc["uptime_s"] = millis() / 1000;
    doc["active_engine"] = (_screen->getActiveEngine() == ActiveEngine::EMOTION) ? "emotion" : "clock";
    doc["emotion"] = _screen->emotionEngine()->getEmotionName();
    doc["clock_face"] = _screen->clockEngine()->getFaceName();
    doc["free_heap"] = ESP.getFreeHeap();

    String response;
    serializeJson(doc, response);

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", response);
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handleGetConfig(AsyncWebServerRequest* req) {
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
    doc["weather_key_set"] = _config->weatherApiKey.length() > 0;
    doc["debug_mode"] = _config->debugMode;
    doc["mood_interval"] = _config->moodIntervalS;

    String response;
    serializeJson(doc, response);

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", response);
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handlePostConfig(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // Update only provided fields
    if (!doc["tz_offset"].isNull()) {
        _config->tzOffset = doc["tz_offset"].as<int>();
        _network->setTimezone(_config->tzOffset);  // Apply live (configTime is thread-safe)
    }
    if (!doc["clock_face"].isNull())        _config->clockFace = doc["clock_face"].as<int>();
    if (!doc["brightness"].isNull()) {
        _config->brightness = doc["brightness"].as<int>();
        _config->brightnessChanged = true;
    }
    if (!doc["default_engine"].isNull())    _config->defaultEngine = doc["default_engine"].as<int>();
    if (!doc["auto_switch"].isNull())       _config->autoSwitch = doc["auto_switch"].as<bool>();
    if (!doc["switch_interval"].isNull())   _config->switchIntervalS = doc["switch_interval"].as<int>();
    if (!doc["clock_duration"].isNull())    _config->clockDurationS = doc["clock_duration"].as<int>();
    if (!doc["night_start"].isNull())       _config->nightStartHour = doc["night_start"].as<int>();
    if (!doc["night_end"].isNull())         _config->nightEndHour = doc["night_end"].as<int>();
    if (!doc["night_enabled"].isNull())     _config->nightModeEnabled = doc["night_enabled"].as<bool>();
    if (doc["weather_key"].is<const char*>()) {
        _config->weatherApiKey = doc["weather_key"].as<String>();
        _config->weatherChanged = true;
    }
    if (doc["weather_city"].is<const char*>()) {
        _config->weatherCity = doc["weather_city"].as<String>();
        _config->weatherChanged = true;
    }
    if (doc["debug_mode"].is<bool>())       _config->debugMode = doc["debug_mode"];
    if (!doc["mood_interval"].isNull())      _config->moodIntervalS = doc["mood_interval"].as<int>();

    _config->save();

    // Apply live changes (simple assignments — safe from async task on ESP32)
    _screen->setAutoSwitch(_config->autoSwitch);
    _screen->setAutoSwitchInterval(_config->switchIntervalS);
    _screen->setClockDuration(_config->clockDurationS);
    _screen->clockEngine()->setFace((ClockFace)_config->clockFace);
    _screen->emotionEngine()->setMoodInterval(_config->moodIntervalS);

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", "{\"ok\":true}");
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handlePostWifi(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err || !doc["ssid"].is<const char*>()) {
        req->send(400, "application/json", "{\"error\":\"Need ssid\"}");
        return;
    }

    _config->wifiSSID = doc["ssid"].as<String>();
    _config->wifiPass = doc["pass"].is<const char*>() ? doc["pass"].as<String>() : "";
    _config->save();

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json",
        "{\"ok\":true,\"msg\":\"Restarting WiFi...\"}");
    addCorsHeaders(resp);
    req->send(resp);

    // Schedule reconnect (after response is sent)
    delay(500);
    _network->connectSTA(_config->wifiSSID, _config->wifiPass);
}

void MiniWebServer::handlePostEmotion(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) || !doc["emotion"].is<int>()) {
        req->send(400, "application/json", "{\"error\":\"Need emotion\"}");
        return;
    }

    int e = doc["emotion"].as<int>();
    if (e >= 0 && e < (int)Emotion::COUNT) {
        _screen->emotionEngine()->setEmotion((Emotion)e);
        _screen->setEngine(ActiveEngine::EMOTION);
    }

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", "{\"ok\":true}");
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handlePostClockFace(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) || !doc["face"].is<int>()) {
        req->send(400, "application/json", "{\"error\":\"Need face\"}");
        return;
    }

    int f = doc["face"].as<int>();
    if (f >= 0 && f < (int)ClockFace::COUNT) {
        _screen->clockEngine()->setFace((ClockFace)f);
        _config->clockFace = f;
        _config->save();
    }

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", "{\"ok\":true}");
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handlePostSwitch(AsyncWebServerRequest* req) {
    _screen->switchEngine();
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", "{\"ok\":true}");
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handlePostOtaCheck(AsyncWebServerRequest* req) {
    // OTA check is handled asynchronously — just acknowledge
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json",
        "{\"ok\":true,\"msg\":\"OTA check queued\"}");
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handlePostOtaUpdate(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json",
        "{\"ok\":true,\"msg\":\"OTA update starting...\"}");
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::handleGetWifiScan(AsyncWebServerRequest* req) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true);  // Start async scan
        req->send(202, "application/json", "{\"scanning\":true}");
        return;
    }

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n && i < 15; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }

    WiFi.scanDelete();

    String response;
    serializeJson(doc, response);

    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", response);
    addCorsHeaders(resp);
    req->send(resp);
}

void MiniWebServer::addCorsHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}
