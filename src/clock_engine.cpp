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

void ClockEngine::renderAnalog(DisplayType& d) {
    drawStatusBar(d);

    const char* hours[] = {"TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"};
    const char* mins[] = {"", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "QUARTER", "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN", "TWENTY", "TWENTY-ONE", "TWENTY-TWO", "TWENTY-THREE", "TWENTY-FOUR", "TWENTY-FIVE", "TWENTY-SIX", "TWENTY-SEVEN", "TWENTY-EIGHT", "TWENTY-NINE", "HALF"};
    
    uint8_t dispH = _hour % 12;
    uint8_t nextH = (_hour + 1) % 12;
    
    char l1[32] = "IT IS";
    char l2[32] = "";
    char l3[32] = "";
    char l4[32] = "";
    
    uint8_t m = _minute;
    if (m == 0) {
        strcpy(l2, hours[dispH]);
        strcpy(l3, "O'CLOCK");
    } else if (m <= 30) {
        strcpy(l2, mins[m]);
        if (m == 15 || m == 30) {
            strcpy(l3, "PAST");
        } else if (m == 1) {
            strcpy(l3, "MINUTE PAST");
        } else {
            strcpy(l3, "MINUTES PAST");
        }
        strcpy(l4, hours[dispH]);
    } else {
        uint8_t toM = 60 - m;
        strcpy(l2, mins[toM]);
        if (toM == 15) {
            strcpy(l3, "TO");
        } else if (toM == 1) {
            strcpy(l3, "MINUTE TO");
        } else {
            strcpy(l3, "MINUTES TO");
        }
        strcpy(l4, hours[nextH]);
    }

    int16_t startX = 18; 
    
    // Mathematically perfect optical baselines
    int16_t y1 = 11;
    int16_t y2 = 28;
    int16_t y3 = 41;
    int16_t y4 = 60;
    
#if ENABLE_BATTERY_MODULE
    // Perfect optical baselines for constrained 54px height
    y1 = 9;
    y2 = 24;
    y3 = 35;
    y4 = 52;
#endif
    
    // Spread evenly when l4 is empty (o'clock case)
    if (strlen(l4) == 0) {
        y1 += 6;
        y2 += 9;
        y3 += 12;
    }
    
    // Line 1: IT IS
    u8g2Fonts.setFont(FONT_MEDIUM);
    u8g2Fonts.setCursor(startX, y1);
    u8g2Fonts.print(l1);
    
    // Line 2: Minutes
    if (strlen(l2) > 0) {
        u8g2Fonts.setFont(FONT_LARGE_TEMP);
        if (startX + u8g2Fonts.getUTF8Width(l2) > 124) {
            u8g2Fonts.setFont(FONT_MEDIUM); // Fallback if word is too long
        }
        u8g2Fonts.setCursor(startX, y2);
        u8g2Fonts.print(l2);
    }
    
    // Line 3: PAST / TO
    if (strlen(l3) > 0) {
        u8g2Fonts.setFont(FONT_MEDIUM);
        u8g2Fonts.setCursor(startX, y3);
        u8g2Fonts.print(l3);
    }
    
    // Line 4: Hour
    if (strlen(l4) > 0) {
        u8g2Fonts.setFont(FONT_LARGE_NUM);
        if (startX + u8g2Fonts.getUTF8Width(l4) > 124) {
            u8g2Fonts.setFont(FONT_LARGE_TEMP);
        }
        u8g2Fonts.setCursor(startX, y4);
        u8g2Fonts.print(l4);
    }
}

// --- Minimal Face (7-Segment) ---

static void drawBeveledSegment(DisplayType& d, int16_t x, int16_t y, int16_t L, int16_t T, bool isVert) {
    // x, y is the top-left of the bounding box. L is length, T is thickness.
    int16_t hT = T / 2;
    
    if (L <= T) {
        // Draw diamond
        int16_t cx = x + L/2;
        int16_t cy = y + T/2;
        d.fillTriangle(cx - hT, cy, cx, cy - hT, cx + hT, cy, DISPLAY_WHITE);
        d.fillTriangle(cx - hT, cy, cx + hT, cy, cx, cy + hT, DISPLAY_WHITE);
        return;
    }
    
    if (!isVert) {
        // Horizontal segment
        d.fillTriangle(x, y + hT, x + hT, y, x + hT, y + T - 1, DISPLAY_WHITE);
        d.fillRect(x + hT, y, L - 2*hT, T, DISPLAY_WHITE);
        d.fillTriangle(x + L - 1, y + hT, x + L - 1 - hT, y, x + L - 1 - hT, y + T - 1, DISPLAY_WHITE);
    } else {
        // Vertical segment
        d.fillTriangle(x + hT, y, x, y + hT, x + T - 1, y + hT, DISPLAY_WHITE);
        d.fillRect(x, y + hT, T, L - 2*hT, DISPLAY_WHITE);
        d.fillTriangle(x + hT, y + L - 1, x, y + L - 1 - hT, x + T - 1, y + L - 1 - hT, DISPLAY_WHITE);
    }
}

static void draw7SegMask(DisplayType& d, int16_t X, int16_t Y, uint8_t mask, int16_t W, int16_t T) {
    int16_t g = 1; // 1px gap between segments for LCD look
    int16_t L = W - 2*g; // length of segments
    
    // A (Top)
    if (mask & 0x01) drawBeveledSegment(d, X + g, Y, L, T, false);
    // B (Top Right)
    if (mask & 0x02) drawBeveledSegment(d, X + W - T, Y + g, L, T, true);
    // C (Bottom Right)
    if (mask & 0x04) drawBeveledSegment(d, X + W - T, Y + W - T + g, L, T, true);
    // D (Bottom)
    if (mask & 0x08) drawBeveledSegment(d, X + g, Y + 2*W - 2*T, L, T, false);
    // E (Bottom Left)
    if (mask & 0x10) drawBeveledSegment(d, X, Y + W - T + g, L, T, true);
    // F (Top Left)
    if (mask & 0x20) drawBeveledSegment(d, X, Y + g, L, T, true);
    // G (Middle)
    if (mask & 0x40) drawBeveledSegment(d, X + g, Y + W - T, L, T, false);
}

static void draw7SegDigit(DisplayType& d, int16_t x, int16_t y, uint8_t digit, int16_t S, int16_t T) {
    const uint8_t seg7[10] = {
        0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
        0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
    };
    if (digit <= 9) {
        // Digit '1' only uses the rightmost segments.
        // We shift it left by (S - T) so its physical ink starts exactly at 'x'.
        // This allows true proportional spacing.
        if (digit == 1) {
            x -= (S - T);
        }
        draw7SegMask(d, x, y, seg7[digit], S, T);
    }
}


void ClockEngine::renderMinimal(DisplayType& d) {
    int16_t W = 22;  // Digit Width
    int16_t T = 6;   // Segment thickness
    int16_t gap = 4; // Gap between digits
    int16_t H = 2*W - T; // Total height of a digit (38px)

    // Time calculations
    uint8_t h = _hour % 12;
    if (h == 0) h = 12;
    bool isPM = _hour >= 12;

    uint8_t h1 = h / 10;
    uint8_t h2 = h % 10;
    uint8_t m1 = _minute / 10;
    uint8_t m2 = _minute % 10;

    // Use FONT_MEDIUM for larger AM/PM text
    u8g2Fonts.setFont(FONT_MEDIUM);
    const char* ampmStr = isPM ? "PM" : "AM";
    int16_t ampmW = u8g2Fonts.getUTF8Width(ampmStr);

    auto getDigitWidth = [&](uint8_t digit) {
        return (digit == 1) ? T : W;
    };

    int16_t w_h1 = (h1 > 0) ? getDigitWidth(h1) : 0;
    int16_t w_h2 = getDigitWidth(h2);
    int16_t w_m1 = getDigitWidth(m1);
    int16_t w_m2 = getDigitWidth(m2);

    int16_t totalW = 0;
    if (h1 > 0) totalW += w_h1 + gap;
    totalW += w_h2 + gap; // h2
    totalW += T + gap;    // colon
    totalW += w_m1 + gap; // m1
    totalW += w_m2;       // m2
    totalW += gap + ampmW;

    int16_t startX = (128 - totalW) / 2;
    
    // Vertically center based on whether battery bar is present
#if ENABLE_BATTERY_MODULE
    int16_t y = (54 - H) / 2;
#else
    int16_t y = (SCREEN_HEIGHT - H) / 2;
#endif

    int16_t currX = startX;

    if (h1 > 0) {
        draw7SegDigit(d, currX, y, h1, W, T);
        currX += w_h1 + gap;
    }

    draw7SegDigit(d, currX, y, h2, W, T);
    currX += w_h2 + gap;

    // Colon
    if (_colonVisible) {
        // Draw diamonds in the upper and lower halves
        drawBeveledSegment(d, currX, y + W/2, T, T, false);
        drawBeveledSegment(d, currX, y + H - W/2, T, T, false);
    }
    currX += T + gap;

    draw7SegDigit(d, currX, y, m1, W, T);
    currX += w_m1 + gap;

    draw7SegDigit(d, currX, y, m2, W, T);
    currX += w_m2;

    // AM / PM perfectly aligned with the bottom of the time digits
    u8g2Fonts.setCursor(currX + gap, y + H); 
    u8g2Fonts.print(ampmStr);

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

    // Percentage text
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
