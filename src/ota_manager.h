#pragma once

#include <Arduino.h>

// =============================================================
// OTAManager — GitHub Release OTA Updates
// =============================================================

class OTAManager {
public:
    void begin();
    void update(uint32_t nowMs);

    // Manual triggers
    bool checkForUpdate();      // Returns true if update available
    bool performUpdate();       // Returns true if update started
    void forceCheck();          // Reset timer and check now
    void requestUpdate();       // Request main loop to perform update

    // Status
    String getLatestVersion() const;
    bool isUpdateAvailable() const;
    String getLastError() const;
    int getProgress() const;
    String getState() const;

private:
    String   _latestVersion;
    String   _downloadUrl;
    String   _lastError;
    String   _state = "idle";
    bool     _updateAvailable = false;
    bool     _updateRequested = false;
    int      _progress = 0;
    uint32_t _lastCheckMs = 0;

    bool fetchLatestRelease();
    int compareVersions(const String& v1, const String& v2);
};
