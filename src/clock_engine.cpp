#include "clock_engine.h"
#include "config.h"
#include "bitmaps/icons.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "font_config.h"

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
        toggleSubScreen();
    }

    // Toggle date/IP info line every 3 seconds
    _infoToggleMs += deltaMs;
    if (_infoToggleMs >= 3000) {
        _infoToggleMs = 0;
        _showIP = !_showIP;
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

ClockFace ClockEngine::getFace() const {
    return _face;
}

const char* ClockEngine::getFaceName() const {
    static const char* names[] = { "Digital", "Analog", "Minimal" };
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

void ClockEngine::renderDigital(DisplayType& d) {
    drawStatusBar(d);

    // --- Large time (render HH and MM at fixed positions to prevent shift) ---
    char hourBuf[4], minBuf[4];
    snprintf(hourBuf, sizeof(hourBuf), "%02d", _hour);
    snprintf(minBuf, sizeof(minBuf), "%02d", _minute);

    d.setFont(&FreeSansBold18pt7b);
    d.setTextSize(1);
    d.setTextColor(DISPLAY_WHITE);

    // Measure "00:00" to get a stable total width for centering
    int16_t refX, refY;
    uint16_t refW, refH;
    d.getTextBounds("00:00", 0, 0, &refX, &refY, &refW, &refH);
    int16_t baseX = (SCREEN_WIDTH - refW) / 2 - refX;
    int16_t ty = 38;  // Baseline Y

    // Measure colon width for positioning
    int16_t cX, cY;
    uint16_t cW, cH;
    d.getTextBounds(":", 0, 0, &cX, &cY, &cW, &cH);

    // Measure hour width
    int16_t hX, hY;
    uint16_t hW, hH;
    d.getTextBounds(hourBuf, 0, 0, &hX, &hY, &hW, &hH);

    // Draw hours
    d.setCursor(baseX, ty);
    d.print(hourBuf);

    // Draw colon (blinking) at fixed position after hours with wider spacing
    int16_t colonX = baseX + hW + 5;
    if (_colonVisible) {
        d.setCursor(colonX, ty);
        d.print(':');
    }

    // Draw minutes at fixed position after colon with wider spacing
    int16_t minX = colonX + cW + 5;
    d.setCursor(minX, ty);
    d.print(minBuf);

    // --- Info line below time: alternates between date and IP ---
    const char* infoText;
    char dateBuf[20];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s",
             getDayName(_dow), _day, getMonthName(_month));

    if (_showIP && _wifiConnected) {
        infoText = _ipAddress.c_str();
    } else {
        infoText = dateBuf;
    }

    u8g2Fonts.setFont(FONT_MEDIUM);
    int16_t dbw = u8g2Fonts.getUTF8Width(infoText);
    u8g2Fonts.setCursor((SCREEN_WIDTH - dbw) / 2, 54);
    u8g2Fonts.print(infoText);
}

// --- Analog Face ---
// Layout:
//   [WiFi]              [BAT xx%]
//
//    ╭─────╮
//    │ clock│   Day
//    │ face │   DD
//    ╰─────╯   Mon
//
//               temp°C

void ClockEngine::renderAnalog(DisplayType& d) {
    drawStatusBar(d);

    // Clock center and radius
    const int16_t cx = 40;
    const int16_t cy = 36;
    const int16_t r = 22;

    // Draw clock circle
    d.drawCircle(cx, cy, r, DISPLAY_WHITE);
    d.drawCircle(cx, cy, r + 1, DISPLAY_WHITE);

    // Draw hour markers
    for (int i = 0; i < 12; i++) {
        float angle = i * 30.0f * 3.14159f / 180.0f - 3.14159f / 2.0f;
        int16_t x1 = cx + (int16_t)(cosf(angle) * (r - 2));
        int16_t y1 = cy + (int16_t)(sinf(angle) * (r - 2));
        int16_t x2 = cx + (int16_t)(cosf(angle) * (r - 5));
        int16_t y2 = cy + (int16_t)(sinf(angle) * (r - 5));
        d.drawLine(x1, y1, x2, y2, DISPLAY_WHITE);
    }

    // Center dot
    d.fillCircle(cx, cy, 2, DISPLAY_WHITE);

    // Hour hand
    float hourAngle = ((_hour % 12) + _minute / 60.0f) * 30.0f * 3.14159f / 180.0f - 3.14159f / 2.0f;
    int16_t hx = cx + (int16_t)(cosf(hourAngle) * (r - 10));
    int16_t hy = cy + (int16_t)(sinf(hourAngle) * (r - 10));
    d.drawLine(cx, cy, hx, hy, DISPLAY_WHITE);
    // Thicken hour hand
    d.drawLine(cx + 1, cy, hx + 1, hy, DISPLAY_WHITE);
    d.drawLine(cx, cy + 1, hx, hy + 1, DISPLAY_WHITE);

    // Minute hand
    float minAngle = _minute * 6.0f * 3.14159f / 180.0f - 3.14159f / 2.0f;
    int16_t mx = cx + (int16_t)(cosf(minAngle) * (r - 5));
    int16_t my = cy + (int16_t)(sinf(minAngle) * (r - 5));
    d.drawLine(cx, cy, mx, my, DISPLAY_WHITE);

    // --- Date on the right side (smooth U8g2 fonts) ---
    u8g2Fonts.setFont(FONT_MEDIUM);
    u8g2Fonts.setCursor(74, 26);
    u8g2Fonts.print(getDayName(_dow));

    char dayBuf[4];
    snprintf(dayBuf, sizeof(dayBuf), "%02d", _day);
    u8g2Fonts.setFont(FONT_LARGE_NUM);
    u8g2Fonts.setCursor(74, 44);
    u8g2Fonts.print(dayBuf);

    u8g2Fonts.setFont(FONT_MEDIUM);
    u8g2Fonts.setCursor(74, 54);
    u8g2Fonts.print(getMonthName(_month));
}

// --- Minimal Face ---
// Layout:
//
//
//        HH:MM
//
//   ████████████░░░ 85%

void ClockEngine::renderMinimal(DisplayType& d) {
    // --- Large centered time (fixed positions to prevent colon-blink shift) ---
    char hourBuf[4], minBuf[4];
    snprintf(hourBuf, sizeof(hourBuf), "%02d", _hour);
    snprintf(minBuf, sizeof(minBuf), "%02d", _minute);

    d.setFont(&FreeSansBold18pt7b);
    d.setTextSize(1);
    d.setTextColor(DISPLAY_WHITE);

    // Measure "00:00" for stable centering
    int16_t refX, refY;
    uint16_t refW, refH;
    d.getTextBounds("00:00", 0, 0, &refX, &refY, &refW, &refH);
    int16_t baseX = (SCREEN_WIDTH - refW) / 2 - refX;
    int16_t ty = 36;

    int16_t cX, cY;
    uint16_t cW, cH;
    d.getTextBounds(":", 0, 0, &cX, &cY, &cW, &cH);

    int16_t hX, hY;
    uint16_t hW, hH;
    d.getTextBounds(hourBuf, 0, 0, &hX, &hY, &hW, &hH);

    d.setCursor(baseX, ty);
    d.print(hourBuf);

    int16_t colonX = baseX + hW + 5;
    if (_colonVisible) {
        d.setCursor(colonX, ty);
        d.print(':');
    }

    int16_t minX = colonX + cW + 5;
    d.setCursor(minX, ty);
    d.print(minBuf);

    // --- Battery bar at bottom ---
#if ENABLE_BATTERY_MODULE
    int16_t barX = 14;
    int16_t barY = 54;
    int16_t barW = 80;
    int16_t barH = 8;

    // Outline
    d.drawRoundRect(barX, barY, barW, barH, 2, DISPLAY_WHITE);

    // Fill
    int16_t fillW = (int16_t)((barW - 4) * _battPercent / 100);
    if (fillW > 0) {
        d.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 1, DISPLAY_WHITE);
    }

    // Percentage text (smooth U8g2 font)
    char battBuf[5];
    snprintf(battBuf, sizeof(battBuf), "%d%%", _battPercent);
    u8g2Fonts.setFont(FONT_SMALL);
    u8g2Fonts.setCursor(barX + barW + 4, barY + 8);
    u8g2Fonts.print(battBuf);
#endif
}

// --- Shared UI ---

void ClockEngine::drawStatusBar(DisplayType& d) {
    // WiFi icon (top-left)
    drawWiFiIcon(d, 0, 0);

#if ENABLE_BATTERY_MODULE
    // Battery percentage (top-right, smooth U8g2 font)
    char battBuf[5];
    snprintf(battBuf, sizeof(battBuf), "%d%%", _battPercent);
    u8g2Fonts.setFont(FONT_SMALL);
    int16_t textWidth = u8g2Fonts.getUTF8Width(battBuf);
    u8g2Fonts.setCursor(SCREEN_WIDTH - textWidth, 9);
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
    const uint8_t* icon = _wifiConnected ? icon_wifi_on : icon_wifi_off;
    d.drawBitmap(x, y, icon, 12, 12, DISPLAY_WHITE);
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
        u8g2Fonts.setCursor((SCREEN_WIDTH - mw) / 2, 40);
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
    u8g2Fonts.setFont(FONT_LARGE_TEMP);
    u8g2Fonts.setCursor(60, 42); // Adjusted Y for FONT_LARGE_TEMP baseline
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
