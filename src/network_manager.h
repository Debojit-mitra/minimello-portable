#pragma once

#include <Arduino.h>
#include <WiFi.h>

// =============================================================
// NetworkManager — WiFi + NTP
// =============================================================

enum class NetState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,
    AP_MODE
};

class NetworkManager {
public:
    void begin(const String& ssid, const String& pass, int32_t tzOffset);
    void update(uint32_t nowMs);

    // WiFi control
    void connectSTA(const String& ssid, const String& pass);
    void startAP();
    void disconnect();

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

private:
    NetState _state = NetState::DISCONNECTED;
    String   _ssid;
    String   _pass;
    int32_t  _tzOffset = 0;
    String   _apName;

    uint32_t _connectStartMs = 0;
    uint32_t _lastNtpSyncMs = 0;
    bool     _timeSynced = false;

    void generateAPName();
};
