#include "clock_engine.h"
#include "bitmaps/icons.h"
#include "font_config.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <math.h>

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

    int16_t startX = (128 - totalW) / 2 + _pixelShiftX;
    
    // Vertically center based on whether battery bar is present
#if ENABLE_BATTERY_MODULE
    int16_t y = (54 - H) / 2 + _pixelShiftY;
#else
    int16_t y = (SCREEN_HEIGHT - H) / 2 + _pixelShiftY;
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
    int16_t barX = 14 + _pixelShiftX;
    int16_t barY = 54 + _pixelShiftY;
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

