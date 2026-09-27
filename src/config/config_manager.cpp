#include "config/config_manager.h"
#include "config.h"
#include "debug_config.h"
#include <WiFi.h>
#include "power/power_manager.h"

// =============================================================
// ConfigManager — Implementation
// =============================================================

void ConfigManager::begin() {
    prefs.begin("minimello", false);  // namespace, read-write

    // --- System ID (MAC based) ---
    macId = prefs.getString("device_id", ""); // Reuse existing key
    if (macId.isEmpty()) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[13];
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        macId = String(macStr);
        prefs.putString("device_id", macId);
    }

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
    oledProtectionEnabled = prefs.getBool("oled_prot", false);
    moodIntervalS   = prefs.getUShort("mood_int", 300);  // Default 5 minutes
    nightStartHour  = prefs.getUChar("night_start", NIGHT_START_HOUR);
    nightEndHour    = prefs.getUChar("night_end", NIGHT_END_HOUR);
    nightModeEnabled= prefs.getBool("night_en", true);
    weatherLat      = prefs.getFloat("weather_lat", 0.0);
    weatherLon      = prefs.getFloat("weather_lon", 0.0);
    weatherCity     = prefs.getString("weather_city", "");
    userName        = prefs.getString("user_name", "");
    debugMode       = prefs.getBool("debug_mode", false);

    // Apply debug mode overrides if compiled with DEBUG_MODE flag
#ifdef DEBUG_MODE
    debugMode = true;
    // Always use debug credentials (override any saved NVS values)
    wifiSSID      = DEBUG_WIFI_SSID;
    wifiPass      = DEBUG_WIFI_PASS;
    weatherLat    = DEBUG_WEATHER_LAT;
    weatherLon    = DEBUG_WEATHER_LON;
    weatherCity   = DEBUG_WEATHER_CITY;
    tzOffset      = DEBUG_TZ_OFFSET;
#endif
}

void ConfigManager::save() {
    prefs.putString("device_id",    macId);
    prefs.putString("wifi_ssid",    wifiSSID);
    prefs.putString("wifi_pass",    wifiPass);
    prefs.putInt("tz_offset",       tzOffset);
    prefs.putUChar("clock_face",    clockFace);
    prefs.putUChar("brightness",    brightness);
    prefs.putUChar("def_engine",    defaultEngine);
    prefs.putBool("auto_switch",    autoSwitch);
    prefs.putUShort("switch_int",   switchIntervalS);
    prefs.putUShort("clock_dur",    clockDurationS);
    prefs.putBool("oled_prot",      oledProtectionEnabled);
    prefs.putUShort("mood_int",     moodIntervalS);
    prefs.putUChar("night_start",   nightStartHour);
    prefs.putUChar("night_end",     nightEndHour);
    prefs.putBool("night_en",       nightModeEnabled);
    prefs.putFloat("weather_lat",   weatherLat);
    prefs.putFloat("weather_lon",   weatherLon);
    prefs.putString("weather_city", weatherCity);
    prefs.putString("user_name",    userName);
    prefs.putBool("debug_mode",     debugMode);
}

void ConfigManager::resetToDefaults() {
    prefs.clear();
    loadDefaults();
    save();
}

void ConfigManager::loadDefaults() {
    // Note: We deliberately don't reset macId so it remains permanent across factory resets
    wifiSSID        = "";
    wifiPass        = "";
    tzOffset        = DEFAULT_TZ_OFFSET;
    clockFace       = 0;
    brightness      = 255;
    defaultEngine   = 0;
    autoSwitch      = true;
    switchIntervalS = AUTO_SWITCH_INTERVAL_S;
    clockDurationS  = CLOCK_DISPLAY_DURATION_S;
    oledProtectionEnabled = false;
    moodIntervalS   = 300;
    nightStartHour  = NIGHT_START_HOUR;
    nightEndHour    = NIGHT_END_HOUR;
    nightModeEnabled= true;
    weatherLat      = 0.0;
    weatherLon      = 0.0;
    weatherCity     = "";
    userName        = "";
    debugMode       = false;
}

String ConfigManager::getDeviceId() {
    extern PowerManager powerMgr;
    String typeCode = powerMgr.hasBattery() ? "02" : "01";
    String shortMac = macId.length() >= 4 ? macId.substring(macId.length() - 4) : macId;
    return "MIME" + typeCode + "-" + shortMac;
}
