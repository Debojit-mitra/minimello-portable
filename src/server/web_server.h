#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "config/config_manager.h"
#include "screen/screen_manager.h"
#include "power/power_manager.h"
#include "network/network_manager.h"

// =============================================================
// WebServer — AsyncWebServer + Captive Portal
// =============================================================
// Serves the WebUI from PROGMEM (gzipped) and provides REST API for
// configuration, status, and control.

class MiniWebServer {
public:
    void begin(ConfigManager* config,
               ScreenManager* screen,
               PowerManager* power,
               NetworkManager* network);
    void enableCaptivePortal();
    void update();

private:
    AsyncWebServer  _server{80};
    ConfigManager*  _config = nullptr;
    ScreenManager*  _screen = nullptr;
    PowerManager*   _power = nullptr;
    NetworkManager* _network = nullptr;
    DNSServer*      _dns = nullptr;

    // Route setup
    void setupStaticRoutes();
    void setupAPIRoutes();

    // API handlers
    void handleGetStatus(AsyncWebServerRequest* req);
    void handleGetConfig(AsyncWebServerRequest* req);
    void handlePostConfig(AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void handlePostWifi(AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void handlePostEmotion(AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void handlePostClockFace(AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void handlePostSwitch(AsyncWebServerRequest* req);
    void handlePostOtaCheck(AsyncWebServerRequest* req);
    void handlePostOtaUpdate(AsyncWebServerRequest* req);
    void handleGetOtaProgress(AsyncWebServerRequest* req);
    void handlePostRestart(AsyncWebServerRequest* req);
    void handleGetWifiScan(AsyncWebServerRequest* req);

    // CORS header helper
    void addCorsHeaders(AsyncWebServerResponse* response);
};
