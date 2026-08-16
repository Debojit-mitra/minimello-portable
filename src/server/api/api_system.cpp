#include "config.h"
#include "logger.h"
#include "ota/ota_manager.h"
#include "server/web_server.h"
#include "version.h"
#include "web_ui_data.h"
#include <ArduinoJson.h>

extern OTAManager otaMgr;

void MiniWebServer::handlePostWifi(AsyncWebServerRequest *req, uint8_t *data,
                                   size_t len) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc["ssid"].is<const char *>()) {
    req->send(400, "application/json", "{\"error\":\"Need ssid\"}");
    return;
  }

  _config->wifiSSID = doc["ssid"].as<String>();
  _config->wifiPass =
      doc["pass"].is<const char *>() ? doc["pass"].as<String>() : "";
  _config->save();

  AsyncWebServerResponse *resp = req->beginResponse(
      200, "application/json", "{\"ok\":true,\"msg\":\"Restarting WiFi...\"}");
  addCorsHeaders(resp);
  req->send(resp);

  // Schedule reconnect (after response is sent)
  delay(500);
  _network->connectSTA(_config->wifiSSID, _config->wifiPass);
}

void MiniWebServer::handlePostOtaCheck(AsyncWebServerRequest *req) {
  bool hasUpdate = otaMgr.checkForUpdate();
  String msg;
  if (hasUpdate) {
    msg = "Update available: " + otaMgr.getLatestVersion();
  } else {
    if (otaMgr.getLastError().length() > 0) {
      msg = otaMgr.getLastError();
    } else {
      msg = "Firmware is up to date";
    }
  }

  AsyncWebServerResponse *resp = req->beginResponse(
      200, "application/json",
      "{\"ok\":true,\"msg\":\"" + msg +
          "\",\"has_update\":" + (hasUpdate ? "true" : "false") + "}");
  addCorsHeaders(resp);
  req->send(resp);
}

void MiniWebServer::handlePostOtaUpdate(AsyncWebServerRequest *req) {
  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json",
                         "{\"ok\":true,\"msg\":\"OTA update starting...\"}");
  addCorsHeaders(resp);
  req->send(resp);

  otaMgr.requestUpdate();
}

void MiniWebServer::handleGetOtaProgress(AsyncWebServerRequest *req) {
  AsyncWebServerResponse *resp = req->beginResponse(
      200, "application/json",
      "{\"ok\":true,\"progress\":" + String(otaMgr.getProgress()) +
          ",\"state\":\"" + otaMgr.getState() + "\"}");
  addCorsHeaders(resp);
  req->send(resp);
}

void MiniWebServer::handleGetWifiScan(AsyncWebServerRequest *req) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    if (WiFi.getMode() == WIFI_AP) {
      WiFi.mode(WIFI_AP_STA); // Temporarily enable STA to perform scan
    }
    WiFi.scanNetworks(true); // Start async scan
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

  if (WiFi.getMode() == WIFI_AP_STA && _config->wifiSSID.length() == 0) {
    WiFi.mode(WIFI_AP); // Switch back to pure AP to prevent radio instability
  }

  String response;
  serializeJson(doc, response);

  AsyncWebServerResponse *resp =
      req->beginResponse(200, "application/json", response);
  addCorsHeaders(resp);
  req->send(resp);
}
