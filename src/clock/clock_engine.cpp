#include "clock_engine.h"

#include "config.h"
#include "bitmaps/icons.h"
#include "power/power_manager.h"
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

    // Auto toggle between sub-screens
    // Normal: 2 screens (Time, Weather) in 10s = 5s each
    // Charging: 3 screens (Time, Weather, Charging) in 10s = 3.3s each
    uint32_t switchInterval = (_powerSource == PowerSource::CHARGING || _powerSource == PowerSource::USB_POWERED) ? 3300 : 5000;

    _screenSwitchTimer += deltaMs;
    if (_screenSwitchTimer >= switchInterval) {
        _screenSwitchTimer = 0;
        // Prevent glitch where screen flashes for a fraction of a second 
        // right before global ScreenManager auto-switches to Emotion
        if (screenMgr.getRemainingTime() > 1000) {
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
    } else if (_currentScreen == DashboardScreen::CHARGING) {
        renderChargingDashboard(display);
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
    } else if (_currentScreen == DashboardScreen::WEATHER) {
        // Only switch to CHARGING screen if we are actively plugged in
        if (_powerSource == PowerSource::CHARGING || _powerSource == PowerSource::USB_POWERED) {
            _currentScreen = DashboardScreen::CHARGING;
        } else {
            _currentScreen = DashboardScreen::TIME;
        }
    } else {
        _currentScreen = DashboardScreen::TIME;
    }
}

void ClockEngine::resetView() {
    _currentScreen = DashboardScreen::TIME;
    _screenSwitchTimer = 0;
}

void ClockEngine::showChargingScreen() {
    _currentScreen = DashboardScreen::CHARGING;
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

bool ClockEngine::setBattery(uint8_t percent, PowerSource source) {
    bool justPluggedIn = (_powerSource != source && source == PowerSource::CHARGING);
    if (justPluggedIn) {
        _currentScreen = DashboardScreen::CHARGING;
        _screenSwitchTimer = 0;
    }
    _battPercent = percent; 
    _powerSource = source;
    return justPluggedIn;
}

void ClockEngine::setWiFi(NetState state, int8_t rssi) {
    _wifiState = state;
    _wifiConnected = (state == NetState::CONNECTED);
    _wifiRSSI = rssi;
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
    if (_powerSource == PowerSource::USB_POWERED) {
        drawBatteryIcon(d, SCREEN_WIDTH - 10 + _pixelShiftX, 1 + _pixelShiftY);
    } else {
        drawBatteryIcon(d, SCREEN_WIDTH - 15 + _pixelShiftX, 0 + _pixelShiftY);
    }
#endif
}

void ClockEngine::drawBatteryIcon(DisplayType& d, int16_t x, int16_t y) {
    // USB powered — show USB plug bitmap
    if (_powerSource == PowerSource::USB_POWERED) {
        d.drawBitmap(x, y, icon_usb_power, 16, 8, DISPLAY_WHITE);
        return;
    }

    bool blinkOn = (millis() / 500) % 2 == 0;

    // Low battery warning blink (flash the whole icon)
    if (_powerSource == PowerSource::BATTERY && _battPercent <= 10) {
        if (!blinkOn) return; 
    }

    // --- Programmatic battery icon with 4 fill bars ---
    // Layout (14px wide × 8px tall):
    //   ┌────────────┐┐
    //   │ ■  ■  ■  ■ ││  ← 4 bars (2px wide each, 1px gaps)
    //   └────────────┘┘
    d.drawRect(x, y + 1, 13, 6, DISPLAY_WHITE);   // Body outline
    d.fillRect(x + 13, y + 3, 1, 2, DISPLAY_WHITE); // Positive terminal nub

    // Number of solid (fully charged) bars
    int solidBars = 0;
    if (_powerSource == PowerSource::CHARGING) {
        // While charging, one bar is always blinking (the one currently filling)
        if      (_battPercent >= 80) solidBars = 3; // 4th blinks
        else if (_battPercent >= 55) solidBars = 2; // 3rd blinks
        else if (_battPercent >= 30) solidBars = 1; // 2nd blinks
        else                         solidBars = 0; // 1st blinks
    } else {
        if      (_battPercent >= 75) solidBars = 4;
        else if (_battPercent >= 50) solidBars = 3;
        else if (_battPercent >= 25) solidBars = 2;
        else if (_battPercent >  5)  solidBars = 1;
    }

    // Bar x-offsets inside the body (2px wide, 1px gap between)
    const int16_t offsets[] = {1, 4, 7, 10};

    for (int i = 0; i < 4; i++) {
        if (i < solidBars) {
            d.fillRect(x + offsets[i], y + 2, 2, 4, DISPLAY_WHITE); // Solid
        } else if (i == solidBars && _powerSource == PowerSource::CHARGING) {
            if (blinkOn) d.fillRect(x + offsets[i], y + 2, 2, 4, DISPLAY_WHITE); // Blinking
        }
    }
}

void ClockEngine::drawWiFiIcon(DisplayType& d, int16_t x, int16_t y) {
    if (_wifiState == NetState::CONNECTED) {
        // Clean UI: no icon when connected
        return;
    } else if (_wifiState == NetState::CONNECTING) {
        // Connecting: blink the connected icon
        if ((millis() / 500) % 2 == 0) {
            d.drawBitmap(x, y, icon_wifi_on, 12, 12, DISPLAY_WHITE);
        }
    } else {
        // Disconnected/Off/AP mode: show disconnected icon
        d.drawBitmap(x, y, icon_wifi_off, 12, 12, DISPLAY_WHITE);
    }
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

void ClockEngine::renderChargingDashboard(DisplayType& d) {
    // Draw WiFi icon in top left, but skip the small battery icon since we have a big one
    drawWiFiIcon(d, 0 + _pixelShiftX, 0 + _pixelShiftY);

    const char* title = (_powerSource == PowerSource::USB_POWERED) ? "USB Power" : "Charging";
    u8g2Fonts.setFont(FONT_MEDIUM);
    int16_t tw = u8g2Fonts.getUTF8Width(title);
    u8g2Fonts.setCursor((SCREEN_WIDTH - tw) / 2 + _pixelShiftX, 25 + _pixelShiftY);
    u8g2Fonts.print(title);
    
    if (_powerSource == PowerSource::CHARGING) {
        // Draw a large battery icon in the center (40px wide x 16px tall)
        int16_t bx = (SCREEN_WIDTH - 44) / 2 + _pixelShiftX;
        int16_t by = 36 + _pixelShiftY;

        d.drawRect(bx, by, 40, 16, DISPLAY_WHITE);         // Body
        d.fillRect(bx + 40, by + 4, 3, 8, DISPLAY_WHITE);  // Nub

        int solidBars = 0;
        if      (_battPercent >= 80) solidBars = 3;
        else if (_battPercent >= 55) solidBars = 2;
        else if (_battPercent >= 30) solidBars = 1;
        else                         solidBars = 0;

        // 4 bars, each 7px wide, 2px gaps. Offsets: 2, 11, 20, 29
        const int16_t offsets[] = {2, 11, 20, 29};
        bool blinkOn = (millis() / 500) % 2 == 0;

        for (int i = 0; i < 4; i++) {
            if (i < solidBars) {
                d.fillRect(bx + offsets[i], by + 2, 7, 12, DISPLAY_WHITE); // Solid
            } else if (i == solidBars) {
                if (blinkOn) d.fillRect(bx + offsets[i], by + 2, 7, 12, DISPLAY_WHITE); // Blink
            }
        }
    } else {
        // For USB only, just draw the plug icon in the center
        d.drawBitmap((SCREEN_WIDTH - 16) / 2 + _pixelShiftX, 36 + _pixelShiftY, icon_usb_power, 16, 8, DISPLAY_WHITE);
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
