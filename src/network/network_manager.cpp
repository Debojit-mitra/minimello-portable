#include "network/network_manager.h"
#include "config.h"
#include "logger.h"
#include <esp_wifi.h>
#include <time.h>

// =============================================================
// NetworkManager — Implementation
// =============================================================

void NetworkManager::begin(const String &ssid, const String &pass,
                           int32_t tzOffset) {
  _ssid = ssid;
  _pass = pass;
  _tzOffset = tzOffset;

  generateAPName();

  if (_ssid.length() > 0) {
    connectSTA(_ssid, _pass);
  } else {
    startAP();
  }
}

void NetworkManager::update(uint32_t nowMs) {
  switch (_state) {
  case NetState::CONNECTING:
    if (WiFi.status() == WL_CONNECTED) {
      _state = NetState::CONNECTED;
      LOG_I("WIFI", "Connected: %s", WiFi.localIP().toString().c_str());
      syncTime();
    } else if (nowMs - _connectStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
      LOG_E("WIFI", "Connection timeout, starting AP");
      startAP();
    }
    break;

  case NetState::CONNECTED:
    if (WiFi.status() != WL_CONNECTED) {
      LOG_W("WIFI", "Disconnected, reconnecting...");
      _state = NetState::CONNECTING;
      _connectStartMs = nowMs;
      WiFi.reconnect();
    }
    // Periodic NTP resync
    if (_timeSynced && (nowMs - _lastNtpSyncMs >= NTP_SYNC_INTERVAL_MS)) {
      syncTime();
    }
    break;

  case NetState::AP_MODE:
    // AP mode is persistent until credentials are provided,
    // but if we have an SSID saved, let's continually retry in the background
    if (_ssid.length() > 0) {
      if (WiFi.status() == WL_CONNECTED) {
        LOG_I("WIFI", "Background retry succeeded! Disabling AP.");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _state = NetState::CONNECTED;
        syncTime();
      } else if (nowMs - _lastRetryMs >= 60000) {
        _lastRetryMs = nowMs;
        LOG_I("WIFI", "Retrying connection to %s in background...",
              _ssid.c_str());
        WiFi.disconnect(false, true); // Wipe stale state but keep WiFi on
        WiFi.begin(_ssid.c_str(), _pass.c_str());
      }
    }
    break;

  case NetState::DISCONNECTED:
    break;
  }
}

void NetworkManager::connectSTA(const String &ssid, const String &pass) {
  _ssid = ssid;
  _pass = pass;
  _state = NetState::CONNECTING;
  _connectStartMs = millis();

  LOG_I("WIFI", "Connecting to: %s", _ssid.c_str());

  // Hard reset WiFi PHY to wipe stale WPA3 SAE state on soft reboots
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  WiFi.begin(_ssid.c_str(), _pass.c_str());
}

void NetworkManager::startAP() {
  _state = NetState::AP_MODE;
  _lastRetryMs = millis();

  // Hard reset the radio to clear any stale PHY state from soft
  // reboots/flashing
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(500);

  if (_ssid.length() > 0) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_AP);
  }

  // setTxPower only — do NOT call WiFi.setSleep in AP mode, it breaks DHCP
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.softAP(_apName.c_str(), WIFI_AP_PASSWORD);
  delay(100); // Give DHCP server time to initialize before clients connect

  LOG_I("WIFI", "AP started: %s (IP: %s)", _apName.c_str(),
        WiFi.softAPIP().toString().c_str());
}

void NetworkManager::disconnect() {
  WiFi.disconnect(true);
  _state = NetState::DISCONNECTED;
}

void NetworkManager::syncTime() {
  configTime(_tzOffset, 0, NTP_SERVER, "time.nist.gov");
  _lastNtpSyncMs = millis();
  _timeSynced = true;
  LOG_I("TIME", "NTP sync requested");
}

bool NetworkManager::getLocalTime(struct tm &timeinfo) {
  return ::getLocalTime(&timeinfo, 100); // 100ms timeout
}

NetState NetworkManager::getState() const { return _state; }

bool NetworkManager::isConnected() const {
  return _state == NetState::CONNECTED;
}

int8_t NetworkManager::getRSSI() const {
  if (_state == NetState::CONNECTED) {
    return (int8_t)WiFi.RSSI();
  }
  return -100;
}

String NetworkManager::getIP() const {
  if (_state == NetState::CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (_state == NetState::AP_MODE) {
    return WiFi.softAPIP().toString();
  }
  return "0.0.0.0";
}

String NetworkManager::getAPName() const { return _apName; }

void NetworkManager::setTimezone(int32_t offsetSeconds) {
  _tzOffset = offsetSeconds;
  if (_timeSynced) {
    syncTime(); // Re-sync with new offset
  }
}

bool NetworkManager::isTimeSynced() const { return _timeSynced; }

void NetworkManager::generateAPName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  _apName = String(WIFI_AP_PREFIX) + suffix;
}
