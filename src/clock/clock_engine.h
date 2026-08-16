#pragma once

#include <Arduino.h>
#include "display_config.h"
#include <U8g2_for_Adafruit_GFX.h>

extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// =============================================================
// ClockEngine — Multi-face clock display
// =============================================================
// Renders time, date, battery, WiFi status, and weather data
// on the 128×64 OLED. Supports multiple swappable clock faces.

enum class WeatherCondition : uint8_t {
    UNKNOWN = 0,
    CLEAR,
    CLOUDS,
    RAIN,
    SNOW,
    THUNDER,
    MIST
};

struct WeatherData {
    float           tempC = 0;
    int             humidity = 0;
    char            description[32] = {0};
    char            cityName[32] = {0};
    WeatherCondition condition = WeatherCondition::UNKNOWN;
    bool            valid = false;
    uint32_t        lastUpdateMs = 0;
};

enum class ClockFace : uint8_t {
    DIGITAL = 0,
    ANALOG_FACE,
    MINIMAL,
    COUNT  // sentinel
};

enum class DashboardScreen : uint8_t {
    TIME = 0,
    WEATHER = 1
};

class ClockEngine {
public:
    void begin();
    void update(uint32_t deltaMs);
    void render(DisplayType& display);

    // Face control
    void setFace(ClockFace face);
    ClockFace getFace() const;
    const char* getFaceName() const;

    // Sub-screen control (Time vs Weather)
    void toggleSubScreen();
    void resetView();

    // Data setters (called by main loop with fresh data)
    void setTime(uint8_t hour, uint8_t minute, uint8_t second);
    void setDate(uint8_t day, uint8_t month, uint16_t year, uint8_t dow);
    void setBattery(uint8_t percent, bool charging);
    void setWiFi(bool connected, int8_t rssi);
    void setWeather(const WeatherData& data);
    void setIP(const String& ip);

    uint32_t getFrameInterval() const;

private:
    ClockFace _face = ClockFace::DIGITAL;
    DashboardScreen _currentScreen = DashboardScreen::TIME;
    uint32_t _screenSwitchTimer = 0;

    // Cached display data
    uint8_t  _hour = 0, _minute = 0, _second = 0;
    uint8_t  _day = 1, _month = 1, _dow = 0;
    uint16_t _year = 2025;
    uint8_t  _battPercent = 0;
    bool     _battCharging = false;
    bool     _wifiConnected = false;
    int8_t   _wifiRSSI = -100;
    WeatherData _weather;

    // Animation
    uint32_t _colonBlinkMs = 0;
    bool     _colonVisible = true;

    // Date/IP info line toggle
    String   _ipAddress = "0.0.0.0";
    uint32_t _infoToggleMs = 0;
    bool     _showIP = false;

    // Pixel Shifting
    int8_t   _pixelShiftX = 0;
    int8_t   _pixelShiftY = 0;
    uint32_t _shiftTimerMs = 0;
    uint8_t  _shiftIndex = 0;

    // Render functions per face
    void renderDigital(DisplayType& d);
    void renderAnalog(DisplayType& d);
    void renderMinimal(DisplayType& d);

    // Render weather dashboard
    void renderWeatherDashboard(DisplayType& d);

    // Shared UI elements
    void drawStatusBar(DisplayType& d);
    void drawBatteryIcon(DisplayType& d, int16_t x, int16_t y);
    void drawWiFiIcon(DisplayType& d, int16_t x, int16_t y);
    void drawWeatherIcon(DisplayType& d, int16_t x, int16_t y);

    const char* getDayName(uint8_t dow);
    const char* getMonthName(uint8_t month);
};
