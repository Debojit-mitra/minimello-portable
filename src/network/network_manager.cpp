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
    // Handle non-blocking PHY startup sequence
    if (_asyncConnectStep == 1) {
      if (nowMs - _asyncConnectTimer >= 1500) {
        WiFi.mode(WIFI_STA);
        _asyncConnectStep = 2;
        _asyncConnectTimer = nowMs;
      }
      break;
    } else if (_asyncConnectStep == 2) {
      if (nowMs - _asyncConnectTimer >= 500) {
        _asyncConnectStep = 0;
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(WIFI_PS_MIN_MODEM);
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        WiFi.begin(_ssid.c_str(), _pass.c_str());
      }
      break;
    }

    if (WiFi.status() == WL_CONNECTED) {
      _state = NetState::CONNECTED;
      _connectionRetries = 0;
      _connectedSinceMs = nowMs;
      _lastActivityMs = nowMs;
      LOG_I("WIFI", "Connected: %s", WiFi.localIP().toString().c_str());
      if (!_timeSynced) {
          syncTime();
      }
    } else if (nowMs - _connectStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
      if (_connectionRetries < 2) {
        _connectionRetries++;
        _connectStartMs = nowMs;
        LOG_W("WIFI", "Connection timeout, retrying (%d/3)...",
              _connectionRetries + 1);

        // Hard reset PHY again on retry
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        
        // Hand off to async state machine
        _asyncConnectStep = 1;
        _asyncConnectTimer = nowMs;
      } else {
        LOG_E("WIFI", "Connection failed after 3 attempts. Starting AP...");
        startAP();
      }
    }
    break;

  case NetState::CONNECTED:
    if (WiFi.status() != WL_CONNECTED) {
      LOG_W("WIFI", "Disconnected, reconnecting...");
      _state = NetState::CONNECTING;
      _connectStartMs = nowMs;
      WiFi.reconnect();
      break;
    }
    // Periodic NTP resync
    if (_timeSynced && (nowMs - _lastNtpSyncMs >= NTP_SYNC_INTERVAL_MS)) {
      syncTime();
    }

    // Duty-cycle: check if we should power down the radio
    if (_dutyCycleEnabled) {
      // Use longer timeout if a web client has been active this wake cycle
      uint32_t idleTimeout = _hasWebClient
          ? WIFI_WEB_IDLE_TIMEOUT_MS    // 3 min after last web request
          : WIFI_ACTIVE_WINDOW_MS;       // 30s for service-only wake

      bool activityIdle = (nowMs - _lastActivityMs >= idleTimeout);
      bool webIdle = !_hasWebClient ||
                     (nowMs - _lastWebActivityMs >= idleTimeout);
      // Ensure minimum uptime of WIFI_ACTIVE_WINDOW_MS before sleeping
      bool minUptimeMet = (nowMs - _connectedSinceMs >= WIFI_ACTIVE_WINDOW_MS);

      if (activityIdle && webIdle && minUptimeMet) {
        sleepWiFi();
      }
    }
    break;

  case NetState::AP_MODE:
    // Handle non-blocking AP startup sequence
    if (_asyncConnectStep == 3) {
      if (nowMs - _asyncConnectTimer >= 1000) {
        _asyncConnectStep = 0;
        if (_ssid.length() > 0) {
          WiFi.mode(WIFI_AP_STA);
        } else {
          WiFi.mode(WIFI_AP);
        }
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
        _asyncConnectStep = 4;
        _asyncConnectTimer = nowMs;
      }
      break;
    } else if (_asyncConnectStep == 4) {
      if (nowMs - _asyncConnectTimer >= 250) {
        bool apStarted = WiFi.softAP(_apName.c_str(), WIFI_AP_PASSWORD, 6, 0, 4);
        _asyncConnectStep = 5;
        _asyncConnectTimer = nowMs;
      }
      break;
    } else if (_asyncConnectStep == 5) {
      if (nowMs - _asyncConnectTimer >= 500) {
        _asyncConnectStep = 0;
        LOG_I("WIFI", "AP started: %s on CH 6", _apName.c_str());
      }
      break;
    }

    // AP mode is persistent until credentials are provided,
    // but if we have an SSID saved, let's continually retry in the background
    if (_ssid.length() > 0) {
      if (WiFi.status() == WL_CONNECTED) {
        LOG_I("WIFI", "Background retry succeeded! Disabling AP.");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _state = NetState::CONNECTED;
        if (!_timeSynced) {
            syncTime();
        }
      } else if (nowMs - _lastRetryMs >= 60000) {
        _lastRetryMs = nowMs;
        LOG_I("WIFI", "Retrying connection to %s in background...",
              _ssid.c_str());
        WiFi.disconnect(false, true); // Wipe stale state but keep WiFi on
        WiFi.begin(_ssid.c_str(), _pass.c_str());
      }
    }
    break;

  case NetState::WIFI_OFF:
    if (_wakeRequested) {
      _wakeRequested = false;
      _hasWebClient = false;
      LOG_I("WIFI", "Duty cycle: waking radio (requested)");
      connectSTA(_ssid, _pass);
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
  _connectionRetries = 0;

  LOG_I("WIFI", "Connecting to: %s", _ssid.c_str());

  // Hard reset WiFi PHY to wipe stale WPA3 SAE state on soft reboots
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  
  // Hand off the 1500ms + 500ms delays to the async state machine in update()
  _asyncConnectStep = 1;
  _asyncConnectTimer = millis();
}

void NetworkManager::startAP() {
  _state = NetState::AP_MODE;
  _lastRetryMs = millis();

  // Reset WiFi state cleanly
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  
  // Hand off the delays to the async state machine in update()
  _asyncConnectStep = 3;
  _asyncConnectTimer = millis();
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
  bool valid = ::getLocalTime(&timeinfo, 100); // 100ms timeout
  if (valid && timeinfo.tm_year > 120) { // Reject dates before 2020
    return true;
  }
  return false;
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

bool NetworkManager::needsTimeSync(uint32_t nowMs) const {
  return _timeSynced && (nowMs - _lastNtpSyncMs >= NTP_SYNC_INTERVAL_MS);
}

// =============================================================
// WiFi Power Management (Duty Cycling)
// =============================================================

void NetworkManager::requestWiFi() {
  _lastActivityMs = millis();
  if (_state == NetState::WIFI_OFF) {
    _wakeRequested = true;
  }
}

void NetworkManager::notifyWebActivity() {
  uint32_t now = millis();
  _lastWebActivityMs = now;
  _lastActivityMs = now;
  _hasWebClient = true;
}

void NetworkManager::enableDutyCycle(bool en) {
  _dutyCycleEnabled = en;
  if (en && _state == NetState::CONNECTED) {
    // Start tracking from now so we don't immediately sleep
    _connectedSinceMs = millis();
    _lastActivityMs = millis();
  }
  LOG_I("WIFI", "Duty cycle %s", en ? "ENABLED" : "DISABLED");
}

bool NetworkManager::isDutyCycleActive() const {
  return _dutyCycleEnabled;
}

void NetworkManager::sleepWiFi() {
  LOG_I("WIFI", "Duty cycle: radio OFF (idle timeout)");
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  _state = NetState::WIFI_OFF;
  _hasWebClient = false;
  _wakeRequested = false;
}

void NetworkManager::generateAPName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  _apName = String(WIFI_AP_PREFIX) + suffix;
}

bool NetworkManager::isAsyncConnecting() const {
  return _asyncConnectStep != 0;
}

