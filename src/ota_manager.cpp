#include "ota_manager.h"
#include "config.h"
#include "logger.h"
#include "version.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// =============================================================
// OTAManager — Implementation
// =============================================================

void OTAManager::begin() {
  _lastCheckMs = millis();
  // Don't check immediately on boot — let things settle
}

void OTAManager::update(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED)
    return;

  if (_updateRequested) {
    _updateRequested = false;
    performUpdate();
  } else if (nowMs - _lastCheckMs >= OTA_CHECK_INTERVAL_MS) {
    checkForUpdate();
  }
}

void OTAManager::requestUpdate() { _updateRequested = true; }

bool OTAManager::checkForUpdate() {
  _lastCheckMs = millis();
  return fetchLatestRelease();
}

bool OTAManager::performUpdate() {
  if (!_updateAvailable || _downloadUrl.length() == 0) {
    _lastError = "No update available";
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    _lastError = "WiFi not connected";
    return false;
  }

  LOG_I("OTA", "Starting update from URL: %s", _downloadUrl.c_str());

  WiFiClientSecure client;
  client.setInsecure(); // GitHub uses HTTPS, skip cert verification

  HTTPUpdate httpUpdate;
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(
      false); // We want to handle reboot manually to show success state

  _progress = 0;
  _state = "updating";
  httpUpdate.onProgress([this](int cur, int total) {
    if (total > 0) {
      _progress = (cur * 100) / total;
      extern void drawOtaProgress(int percent);
      drawOtaProgress(_progress);
    }
  });

  t_httpUpdate_return ret = httpUpdate.update(client, _downloadUrl);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    _lastError = String("Update failed: ") + httpUpdate.getLastErrorString();
    _state = "failed";
    LOG_E("OTA", "%s", _lastError.c_str());
    return false;

  case HTTP_UPDATE_NO_UPDATES:
    _lastError = "No updates";
    _state = "no_updates";
    LOG_I("OTA", "%s", _lastError.c_str());
    return false;

  case HTTP_UPDATE_OK:
    _state = "success";
    LOG_I("OTA", "Update OK! Rebooting in 2 seconds...");
    delay(2000);
    WiFi.disconnect(true, true);
    delay(1000);
    ESP.restart();
    return true;
  }

  return false;
}

void OTAManager::forceCheck() {
  _lastCheckMs = 0; // Will trigger on next update() call
}

String OTAManager::getLatestVersion() const { return _latestVersion; }

bool OTAManager::isUpdateAvailable() const { return _updateAvailable; }

String OTAManager::getLastError() const { return _lastError; }

int OTAManager::getProgress() const { return _progress; }

String OTAManager::getState() const { return _state; }

bool OTAManager::fetchLatestRelease() {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  http.begin(client, OTA_API_URL);
  http.addHeader("Accept", "application/vnd.github.v3+json");
  http.addHeader("User-Agent", "MiniMello-OTA");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    _lastError = String("GitHub API error: ") + String(code);
    LOG_E("OTA", "%s", _lastError.c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Parse response
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    _lastError = String("JSON parse error: ") + err.c_str();
    LOG_E("OTA", "%s", _lastError.c_str());
    return false;
  }

  // Extract tag name (version)
  String tagName = doc["tag_name"].as<String>();
  if (tagName.startsWith("v")) {
    tagName = tagName.substring(1); // Remove leading 'v'
  }
  _latestVersion = tagName;

  // Find .bin asset in release
  _downloadUrl = "";
  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject asset : assets) {
    String name = asset["name"].as<String>();
    if (name.endsWith(".bin")) {
      _downloadUrl = asset["browser_download_url"].as<String>();
      break;
    }
  }

  // Compare versions
  int cmp = compareVersions(_latestVersion, FIRMWARE_VERSION);
  _updateAvailable = (cmp > 0 && _downloadUrl.length() > 0);

  if (_updateAvailable) {
    LOG_I("OTA", "Current: %s Latest: %s -> Update available!",
          FIRMWARE_VERSION, _latestVersion.c_str());
  } else {
    LOG_I("OTA", "Current: %s Latest: %s -> Up to date", FIRMWARE_VERSION,
          _latestVersion.c_str());
  }

  return _updateAvailable;
}

int OTAManager::compareVersions(const String &v1, const String &v2) {
  // Simple semver comparison: "1.2.3" vs "1.2.4"
  int maj1 = 0, min1 = 0, pat1 = 0;
  int maj2 = 0, min2 = 0, pat2 = 0;

  sscanf(v1.c_str(), "%d.%d.%d", &maj1, &min1, &pat1);
  sscanf(v2.c_str(), "%d.%d.%d", &maj2, &min2, &pat2);

  if (maj1 != maj2)
    return maj1 - maj2;
  if (min1 != min2)
    return min1 - min2;
  return pat1 - pat2;
}
