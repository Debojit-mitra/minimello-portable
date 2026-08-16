#include "clock_engine.h"
#include "bitmaps/icons.h"
#include "font_config.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <math.h>

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
    int16_t baseX = (SCREEN_WIDTH - refW) / 2 - refX + _pixelShiftX;
    int16_t ty = 38 + _pixelShiftY;  // Baseline Y

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

