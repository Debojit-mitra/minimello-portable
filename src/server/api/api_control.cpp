#include "server/web_server.h"
#include "config.h"
#include "logger.h"
#include "ota/ota_manager.h"
#include "version.h"
#include "web_ui_data.h"
#include <ArduinoJson.h>

void MiniWebServer::handlePostEmotion(AsyncWebServerRequest *req, uint8_t *data,
                                      size_t len) {
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

  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", "{\"ok\":true}");
  addCorsHeaders(resp);
  req->send(resp);
}

void MiniWebServer::handlePostClockFace(AsyncWebServerRequest *req,
                                        uint8_t *data, size_t len) {
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

  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", "{\"ok\":true}");
  addCorsHeaders(resp);
  req->send(resp);
}

void MiniWebServer::handlePostSwitch(AsyncWebServerRequest *req) {
  _screen->switchEngine();
  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", "{\"ok\":true}");
  addCorsHeaders(resp);
  req->send(resp);
}


void MiniWebServer::handlePostRestart(AsyncWebServerRequest *req) {
  AsyncWebServerResponse *resp = req->beginResponse(
      200, "application/json", "{\"ok\":true,\"msg\":\"Restarting...\"}");
  addCorsHeaders(resp);
  req->send(resp);
  delay(500);
  WiFi.disconnect(true, true);
  delay(1000);
  ESP.restart();
}

