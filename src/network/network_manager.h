#pragma once

#include <Arduino.h>
#include <WiFi.h>

// =============================================================
// NetworkManager — WiFi + NTP + Power Management
// =============================================================

enum class NetState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,
    AP_MODE,
    WIFI_OFF        // Radio powered down (duty-cycle sleep)
};

class NetworkManager {
public:
    void begin(const String& ssid, const String& pass, int32_t tzOffset);
    void update(uint32_t nowMs);

    // WiFi control
    void connectSTA(const String& ssid, const String& pass);
    void startAP();
    void disconnect();

    // WiFi power management (duty cycling)
    void requestWiFi();              // Any module calls this to request WiFi
    void notifyWebActivity();        // Web server calls this on each request
    void enableDutyCycle(bool en);   // Enable/disable duty cycling
    bool isDutyCycleActive() const;  // Is duty cycling enabled?

    // NTP
    void syncTime();
    bool getLocalTime(struct tm& timeinfo);

    // Status
    NetState getState() const;
    bool isConnected() const;
    int8_t getRSSI() const;
    String getIP() const;
    String getAPName() const;

    // Timezone
    void setTimezone(int32_t offsetSeconds);

    // NTP sync status
    bool isTimeSynced() const;
    bool needsTimeSync(uint32_t nowMs) const;

private:
    NetState _state = NetState::DISCONNECTED;
    String   _ssid;
    String   _pass;
    int32_t  _tzOffset = 0;
    String   _apName;

    uint32_t _connectStartMs = 0;
    uint32_t _lastRetryMs = 0;
    uint32_t _lastNtpSyncMs = 0;
    bool     _timeSynced = false;
    uint8_t  _connectionRetries = 0;

    // Async connection tracking to prevent blocking delays
    uint8_t  _asyncConnectStep = 0;
    uint32_t _asyncConnectTimer = 0;
    // Duty-cycle state
    bool     _dutyCycleEnabled = false;
    bool     _wakeRequested = false;
    uint32_t _connectedSinceMs = 0;    // When we entered CONNECTED state
    uint32_t _lastActivityMs = 0;      // Last service/touch activity
    uint32_t _lastWebActivityMs = 0;   // Last web request timestamp
    bool     _hasWebClient = false;    // True after first web request in wake cycle

    void generateAPName();
    void sleepWiFi();                  // Power down radio → WIFI_OFF
};
