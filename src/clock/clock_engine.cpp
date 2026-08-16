#include "clock_engine.h"

#include "config.h"
#include "bitmaps/icons.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "font_config.h"
#include "config/config_manager.h"
#include "logger.h"
#include "screen/screen_manager.h"

extern ConfigManager configMgr;
extern ScreenManager screenMgr;

// =============================================================
// ClockEngine — Implementation
// =============================================================

void ClockEngine::begin() {
    _face = ClockFace::DIGITAL;
    _colonVisible = true;
}

void ClockEngine::update(uint32_t deltaMs) {
    // Blink colon every 500ms
    _colonBlinkMs += deltaMs;
    if (_colonBlinkMs >= 500) {
        _colonBlinkMs -= 500;
        _colonVisible = !_colonVisible;
    }

    // Auto toggle between Time and Weather sub-screens every 5 seconds
    _screenSwitchTimer += deltaMs;
    if (_screenSwitchTimer >= 5000) {
        _screenSwitchTimer = 0;
        // Prevent glitch where weather flashes for a fraction of a second 
        // right before global ScreenManager auto-switches to Emotion
        if (screenMgr.getRemainingTime() > 1500) {
            toggleSubScreen();
        }
    }

    // Toggle date/IP info line every 3 seconds
    _infoToggleMs += deltaMs;
    if (_infoToggleMs >= 5000) {
        _infoToggleMs = 0;
        _showIP = !_showIP;
    }

    // --- Pixel Shifting Logic ---
    if (configMgr.oledProtectionEnabled) {
        _shiftTimerMs += deltaMs;
        if (_shiftTimerMs >= 60000) { // Shift every 1 minute
            _shiftTimerMs = 0;
            _shiftIndex = (_shiftIndex + 1) % 8;
            // 3x3 circular path
            static const int8_t driftX[8] = { 0, 1, 1, 1, 0,-1,-1,-1};
            static const int8_t driftY[8] = {-1,-1, 0, 1, 1, 1, 0,-1};
            _pixelShiftX = driftX[_shiftIndex];
            _pixelShiftY = driftY[_shiftIndex];
            
            #ifdef DEBUG_MODE
            LOG_I("OLED", "Pixel Shifted to offset X:%d Y:%d", _pixelShiftX, _pixelShiftY);
            #endif
        }
    } else {
        _pixelShiftX = 0;
        _pixelShiftY = 0;
        _shiftTimerMs = 0;
    }
}

void ClockEngine::render(DisplayType& display) {
    if (_currentScreen == DashboardScreen::WEATHER) {
        renderWeatherDashboard(display);
    } else {
        switch (_face) {
            case ClockFace::DIGITAL:  renderDigital(display);  break;
            case ClockFace::ANALOG_FACE:   renderAnalog(display);   break;
            case ClockFace::MINIMAL:  renderMinimal(display);  break;
            default:                  renderDigital(display);  break;
        }
    }
}

// --- Face control ---

void ClockEngine::setFace(ClockFace face) {
    _face = face;
}

void ClockEngine::toggleSubScreen() {
    _screenSwitchTimer = 0; // Reset auto-switch timer on manual toggle
    if (_currentScreen == DashboardScreen::TIME) {
        _currentScreen = DashboardScreen::WEATHER;
    } else {
        _currentScreen = DashboardScreen::TIME;
    }
}

void ClockEngine::resetView() {
    _currentScreen = DashboardScreen::TIME;
    _screenSwitchTimer = 0;
}

ClockFace ClockEngine::getFace() const {
    return _face;
}

const char* ClockEngine::getFaceName() const {
    static const char* names[] = { "Digital", "Typographic Word", "Minimal" };
    return names[(uint8_t)_face];
}

// --- Data setters ---

void ClockEngine::setTime(uint8_t hour, uint8_t minute, uint8_t second) {
    _hour = hour; _minute = minute; _second = second;
}

void ClockEngine::setDate(uint8_t day, uint8_t month, uint16_t year, uint8_t dow) {
    _day = day; _month = month; _year = year; _dow = dow;
}

void ClockEngine::setBattery(uint8_t percent, bool charging) {
    _battPercent = percent; _battCharging = charging;
}

void ClockEngine::setWiFi(bool connected, int8_t rssi) {
    _wifiConnected = connected; _wifiRSSI = rssi;
}

void ClockEngine::setWeather(const WeatherData& data) {
    _weather = data;
}

void ClockEngine::setIP(const String& ip) {
    _ipAddress = ip;
}

uint32_t ClockEngine::getFrameInterval() const {
    return 1000 / CLOCK_FPS;
}

// --- Digital Face ---
// Layout:
//   [WiFi]              [BAT xx%]
//
//        HH:MM
//      Day, DD Mon
//
//               temp°C [icon]

void ClockEngine::drawStatusBar(DisplayType& d) {
    // WiFi icon (top-left)
    drawWiFiIcon(d, 0 + _pixelShiftX, 0 + _pixelShiftY);

#if ENABLE_BATTERY_MODULE
    // Battery percentage (top-right, smooth U8g2 font)
    char battBuf[5];
    snprintf(battBuf, sizeof(battBuf), "%d%%", _battPercent);
    u8g2Fonts.setFont(FONT_SMALL);
    int16_t textWidth = u8g2Fonts.getUTF8Width(battBuf);
    u8g2Fonts.setCursor(SCREEN_WIDTH - textWidth + _pixelShiftX, 9 + _pixelShiftY);
    u8g2Fonts.print(battBuf);
#endif
}

void ClockEngine::drawBatteryIcon(DisplayType& d, int16_t x, int16_t y) {
    const uint8_t* icon;
    if (_battPercent >= 80)      icon = icon_battery_full;
    else if (_battPercent >= 50) icon = icon_battery_high;
    else if (_battPercent >= 25) icon = icon_battery_mid;
    else if (_battPercent >= 10) icon = icon_battery_low;
    else                         icon = icon_battery_empty;

    d.drawBitmap(x, y, icon, 16, 8, DISPLAY_WHITE);
}

void ClockEngine::drawWiFiIcon(DisplayType& d, int16_t x, int16_t y) {
    // Only show the icon if Wi-Fi is disconnected/error
    if (_wifiConnected) return;
    d.drawBitmap(x, y, icon_wifi_off, 12, 12, DISPLAY_WHITE);
}

void ClockEngine::drawWeatherIcon(DisplayType& d, int16_t x, int16_t y) {
    const uint8_t* icon = nullptr;
    switch (_weather.condition) {
        case WeatherCondition::CLEAR:   icon = icon_weather_clear;   break;
        case WeatherCondition::CLOUDS:  icon = icon_weather_clouds;  break;
        case WeatherCondition::RAIN:    icon = icon_weather_rain;    break;
        case WeatherCondition::SNOW:    icon = icon_weather_snow;    break;
        case WeatherCondition::THUNDER: icon = icon_weather_thunder; break;
        case WeatherCondition::MIST:    icon = icon_weather_mist;    break;
        default: return;
    }
    // Weather icons are 16x16 but we draw them small (8x8 region)
    // Just draw the icon if we have one
    if (icon) {
        d.drawBitmap(x, y, icon, 16, 16, DISPLAY_WHITE);
    }
}

void ClockEngine::renderWeatherDashboard(DisplayType& d) {
    drawStatusBar(d);

    if (!_weather.valid) {
        const char* msg = "No Weather Data";
        u8g2Fonts.setFont(FONT_MEDIUM);
        int16_t mw = u8g2Fonts.getUTF8Width(msg);
        u8g2Fonts.setCursor((SCREEN_WIDTH - mw) / 2 + _pixelShiftX, 40 + _pixelShiftY);
        u8g2Fonts.print(msg);
        return;
    }

    // City Name (Top Center, below status bar)
    u8g2Fonts.setFont(FONT_LARGE);
    int16_t cw = u8g2Fonts.getUTF8Width(_weather.cityName);
    u8g2Fonts.setCursor((SCREEN_WIDTH - cw) / 2, 16);
    u8g2Fonts.print(_weather.cityName);

    // Weather Icon
    const uint8_t* icon;
    switch (_weather.condition) {
        case WeatherCondition::CLEAR:   icon = icon_weather_clear;   break;
        case WeatherCondition::CLOUDS:  icon = icon_weather_clouds;  break;
        case WeatherCondition::RAIN:    icon = icon_weather_rain;    break;
        case WeatherCondition::SNOW:    icon = icon_weather_snow;    break;
        case WeatherCondition::THUNDER: icon = icon_weather_thunder; break;
        case WeatherCondition::MIST:    icon = icon_weather_mist;    break;
        default:                        icon = icon_weather_clear;   break;
    }
    
    // Draw icon at 1.5x scale (24x24) (Left side)
    int16_t iconX = 22;
    int16_t iconY = 24;
    for (int dy = 0; dy < 24; dy++) {
        int sy = dy * 2 / 3;
        uint8_t row = pgm_read_byte(icon + (sy * 2));
        uint8_t row2 = pgm_read_byte(icon + (sy * 2) + 1);
        uint16_t rowData = (row << 8) | row2;
        for (int dx = 0; dx < 24; dx++) {
            int sx = dx * 2 / 3;
            if (rowData & (0x8000 >> sx)) {
                d.drawPixel(iconX + dx, iconY + dy, DISPLAY_WHITE);
            }
        }
    }

    // Temperature (Right side)
    char tempBuf[12];
    snprintf(tempBuf, sizeof(tempBuf), "%d°C", (int)_weather.tempC);
    u8g2Fonts.setFont(FONT_WEATHER_TEMP);
    u8g2Fonts.setCursor(60, 45); // Adjusted Y for Logisoso16 baseline
    u8g2Fonts.print(tempBuf);

    // Description (Bottom Left, smooth U8g2 font)
    char capDesc[32];
    strncpy(capDesc, _weather.description, sizeof(capDesc));
    if (capDesc[0] >= 'a' && capDesc[0] <= 'z') capDesc[0] -= 32;
    u8g2Fonts.setFont(FONT_MEDIUM);
    u8g2Fonts.setCursor(2, 63);
    u8g2Fonts.print(capDesc);

    // Humidity (Bottom Right, smooth U8g2 font)
    char humBuf[12];
    snprintf(humBuf, sizeof(humBuf), "%d%%", _weather.humidity);
    u8g2Fonts.setFont(FONT_MEDIUM);
    int16_t hw = u8g2Fonts.getUTF8Width(humBuf);
    u8g2Fonts.setCursor(SCREEN_WIDTH - hw - 2, 63);
    u8g2Fonts.print(humBuf);
}

// --- Utility ---

const char* ClockEngine::getDayName(uint8_t dow) {
    static const char* days[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    return days[dow % 7];
}

const char* ClockEngine::getMonthName(uint8_t month) {
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    if (month < 1 || month > 12) return "???";
    return months[month - 1];
}
