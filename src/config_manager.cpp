#include "config_manager.h"
#include "config.h"
#include "debug_config.h"

// =============================================================
// ConfigManager — Implementation
// =============================================================

void ConfigManager::begin() {
    prefs.begin("minimello", false);  // namespace, read-write

    // Load saved values (or defaults if first boot)
    wifiSSID        = prefs.getString("wifi_ssid", "");
    wifiPass        = prefs.getString("wifi_pass", "");
    tzOffset        = prefs.getInt("tz_offset", DEFAULT_TZ_OFFSET);
    clockFace       = prefs.getUChar("clock_face", 0);
    brightness      = prefs.getUChar("brightness", 255);
    defaultEngine   = prefs.getUChar("def_engine", 0);
    autoSwitch      = prefs.getBool("auto_switch", true);
    switchIntervalS = prefs.getUShort("switch_int", AUTO_SWITCH_INTERVAL_S);
    clockDurationS  = prefs.getUShort("clock_dur", CLOCK_DISPLAY_DURATION_S);
    moodIntervalS   = prefs.getUShort("mood_int", 300);  // Default 5 minutes
    nightStartHour  = prefs.getUChar("night_start", NIGHT_START_HOUR);
    nightEndHour    = prefs.getUChar("night_end", NIGHT_END_HOUR);
    nightModeEnabled= prefs.getBool("night_en", true);
    weatherApiKey   = prefs.getString("weather_key", "");
    weatherCity     = prefs.getString("weather_city", "");
    debugMode       = prefs.getBool("debug_mode", false);

    // Apply debug mode overrides if compiled with DEBUG_MODE flag
#ifdef DEBUG_MODE
    debugMode = true;
    // Always use debug credentials (override any saved NVS values)
    wifiSSID      = DEBUG_WIFI_SSID;
    wifiPass      = DEBUG_WIFI_PASS;
    weatherApiKey = DEBUG_WEATHER_API_KEY;
    weatherCity   = DEBUG_WEATHER_CITY;
    tzOffset      = DEBUG_TZ_OFFSET;
#endif
}

void ConfigManager::save() {
    prefs.putString("wifi_ssid",    wifiSSID);
    prefs.putString("wifi_pass",    wifiPass);
    prefs.putInt("tz_offset",       tzOffset);
    prefs.putUChar("clock_face",    clockFace);
    prefs.putUChar("brightness",    brightness);
    prefs.putUChar("def_engine",    defaultEngine);
    prefs.putBool("auto_switch",    autoSwitch);
    prefs.putUShort("switch_int",   switchIntervalS);
    prefs.putUShort("clock_dur",    clockDurationS);
    prefs.putUShort("mood_int",     moodIntervalS);
    prefs.putUChar("night_start",   nightStartHour);
    prefs.putUChar("night_end",     nightEndHour);
    prefs.putBool("night_en",       nightModeEnabled);
    prefs.putString("weather_key",  weatherApiKey);
    prefs.putString("weather_city", weatherCity);
    prefs.putBool("debug_mode",     debugMode);
}

void ConfigManager::resetToDefaults() {
    prefs.clear();
    loadDefaults();
    save();
}

void ConfigManager::loadDefaults() {
    wifiSSID        = "";
    wifiPass        = "";
    tzOffset        = DEFAULT_TZ_OFFSET;
    clockFace       = 0;
    brightness      = 255;
    defaultEngine   = 0;
    autoSwitch      = true;
    switchIntervalS = AUTO_SWITCH_INTERVAL_S;
    clockDurationS  = CLOCK_DISPLAY_DURATION_S;
    moodIntervalS   = 300;
    nightStartHour  = NIGHT_START_HOUR;
    nightEndHour    = NIGHT_END_HOUR;
    nightModeEnabled= true;
    weatherApiKey   = "";
    weatherCity     = "";
    debugMode       = false;
}
