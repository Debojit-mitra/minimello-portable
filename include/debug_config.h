#pragma once

// =============================================================
// Minimello Portable — Debug Mode Configuration
// =============================================================
// When DEBUG_MODE is defined in platformio.ini build_flags,
// these values are used as defaults instead of requiring WebUI
// setup. You can still override them via WebUI at runtime.
//
// To enable: uncomment "-DDEBUG_MODE" in platformio.ini
// =============================================================

#ifdef DEBUG_MODE

// #define DEBUG_WIFI_SSID "hana_iot"
// #define DEBUG_WIFI_PASS "*fQF#vW6yBOXuj"
#define DEBUG_WIFI_SSID "hana_iot"
#define DEBUG_WIFI_PASS "*fQF#vW6yBOXuj"
#define DEBUG_WEATHER_API_KEY "f6d78b14b4ec9204b66cda5b2513a721"
#define DEBUG_WEATHER_CITY "Guwahati"
#define DEBUG_TZ_OFFSET 19800 // IST (+5:30)

#endif
