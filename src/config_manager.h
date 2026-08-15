#pragma once

#include <Preferences.h>
#include <Arduino.h>

// =============================================================
// ConfigManager — Persistent settings via ESP32 NVS
// =============================================================

class ConfigManager {
public:
    void begin();
    void save();
    void resetToDefaults();

    // --- WiFi ---
    String   wifiSSID;
    String   wifiPass;

    // --- Timezone ---
    int32_t  tzOffset;          // seconds from UTC

    // --- Display ---
    uint8_t  clockFace;         // active clock face index
    uint8_t  brightness;        // OLED brightness 0-255
    volatile bool brightnessChanged = false;  // set by WebUI, consumed by main loop
    uint8_t  defaultEngine;     // 0 = emotion, 1 = clock

    // --- Auto-Switch ---
    bool     autoSwitch;        // enable engine auto-switching
    uint16_t switchIntervalS;   // seconds between switches
    uint16_t clockDurationS;    // seconds to show clock

    // --- Personality / Mood ---
    uint16_t moodIntervalS;     // seconds between ambient mood changes

    // --- Night Mode ---
    volatile uint8_t nightStartHour;
    volatile uint8_t nightEndHour;
    volatile bool    nightModeEnabled;

    // --- Weather ---
    String   weatherApiKey;
    String   weatherCity;
    volatile bool weatherChanged = false;   // set by WebUI, consumed by main loop

    // --- Debug ---
    bool     debugMode;

private:
    Preferences prefs;
    void loadDefaults();
};
