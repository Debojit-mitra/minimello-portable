#include "server/web_server.h"

#include "config.h"
#include "logger.h"
#include "ota/ota_manager.h"
#include "power/power_manager.h"
#include "version.h"
#include "weather/weather_service.h"
#include "web_ui_data.h"
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>

extern OTAManager otaMgr;

// =============================================================
// MiniWebServer — Implementation
// =============================================================

void MiniWebServer::begin(ConfigManager *config, ScreenManager *screen,
                          PowerManager *power, NetworkManager *network) {
  _config = config;
  _screen = screen;
  _power = power;
  _network = network;

  setupAPIRoutes(); // Register API handlers first so they match before static
                    // files
  setupStaticRoutes();

  _server.begin();
  LOG_I("WEB", "Web server started");

  if (MDNS.begin("minimello")) {
    MDNS.addService("http", "tcp", 80);
    LOG_I("WEB", "mDNS responder started at minimello.local");
  } else {
    LOG_E("WEB", "Error starting mDNS");
  }
}

void MiniWebServer::enableCaptivePortal() {
  _dns = new DNSServer();
  _dns->start(53, "*", WiFi.softAPIP());
  LOG_I("WEB", "Captive portal DNS started");

  // Redirect all unknown requests to index.html
  _server.onNotFound([](AsyncWebServerRequest *req) { req->redirect("/"); });
}

void MiniWebServer::update() {
  if (_dns) {
    _dns->processNextRequest();
  }
}

void MiniWebServer::setupStaticRoutes() {
  // Serve WebUI from PROGMEM (gzipped)

  // Index HTML
  auto serveIndex = [](AsyncWebServerRequest *req) {
    AsyncWebServerResponse *res = req->beginResponse(
        200, "text/html", web_index_html, web_index_html_len);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
  };
  _server.on("/", HTTP_GET, serveIndex);
  _server.on("/index.html", HTTP_GET, serveIndex);

  // CSS
  _server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncWebServerResponse *res =
        req->beginResponse(200, "text/css", web_style_css, web_style_css_len);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
  });

  // JS
  _server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncWebServerResponse *res = req->beginResponse(
        200, "application/javascript", web_app_js, web_app_js_len);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
  });
}

void MiniWebServer::setupAPIRoutes() {
  // --- GET routes ---
  _server.on("/api/status", HTTP_GET,
             [this](AsyncWebServerRequest *req) { handleGetStatus(req); });

  _server.on("/api/config", HTTP_GET,
             [this](AsyncWebServerRequest *req) { handleGetConfig(req); });

  _server.on("/api/wifi/scan", HTTP_GET,
             [this](AsyncWebServerRequest *req) { handleGetWifiScan(req); });

  // --- POST routes (with body) ---
  // Config update
  _server.on(
      "/api/config", HTTP_POST,
      [](AsyncWebServerRequest *req) { /* handled in body cb */ }, nullptr,
      [this](AsyncWebServerRequest *req, uint8_t *data, size_t len,
             size_t index, size_t total) {
        if (index == 0)
          handlePostConfig(req, data, len);
      });

  // WiFi credentials
  _server.on(
      "/api/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
      [this](AsyncWebServerRequest *req, uint8_t *data, size_t len,
             size_t index, size_t total) {
        if (index == 0)
          handlePostWifi(req, data, len);
      });

  // Set emotion
  _server.on(
      "/api/emotion", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
      [this](AsyncWebServerRequest *req, uint8_t *data, size_t len,
             size_t index, size_t total) {
        if (index == 0)
          handlePostEmotion(req, data, len);
      });

  // Set clock face
  _server.on(
      "/api/clock/face", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
      [this](AsyncWebServerRequest *req, uint8_t *data, size_t len,
             size_t index, size_t total) {
        if (index == 0)
          handlePostClockFace(req, data, len);
      });

  // Toggle engine
  _server.on("/api/switch", HTTP_POST,
             [this](AsyncWebServerRequest *req) { handlePostSwitch(req); });

  // OTA
  _server.on("/api/ota/check", HTTP_POST,
             [this](AsyncWebServerRequest *req) { handlePostOtaCheck(req); });

  _server.on("/api/ota/update", HTTP_POST,
             [this](AsyncWebServerRequest *req) { handlePostOtaUpdate(req); });

  _server.on("/api/ota/progress", HTTP_GET,
             [this](AsyncWebServerRequest *req) { handleGetOtaProgress(req); });

  // Restart device
  _server.on("/api/restart", HTTP_POST,
             [this](AsyncWebServerRequest *req) { handlePostRestart(req); });

  // CORS preflight
  _server.on("/api/*", HTTP_OPTIONS, [this](AsyncWebServerRequest *req) {
    AsyncWebServerResponse *resp = req->beginResponse(200);
    addCorsHeaders(resp);
    req->send(resp);
  });
}

// --- API Handlers ---

void MiniWebServer::addCorsHeaders(AsyncWebServerResponse *response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}
