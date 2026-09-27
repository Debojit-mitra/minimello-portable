#include "ui/ui_components.h"
#include <U8g2_for_Adafruit_GFX.h>
#include "font_config.h"

extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void UIComponents::drawProgressBar(DisplayType& display, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t progressPercent, bool rounded) {
    if (progressPercent > 100) progressPercent = 100;
    
    // Draw outline
    if (rounded) {
        display.drawRoundRect(x, y, w, h, 3, DISPLAY_WHITE);
    } else {
        display.drawRect(x, y, w, h, DISPLAY_WHITE);
    }
    
    // Draw fill
    int16_t fillW = (int16_t)((w - 4) * progressPercent / 100);
    if (fillW > 0) {
        if (rounded) {
            display.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, DISPLAY_WHITE);
        } else {
            display.fillRect(x + 2, y + 2, fillW, h - 4, DISPLAY_WHITE);
        }
    }
}

void UIComponents::drawRestartWarning(DisplayType& display, uint8_t progressPercent, bool clearScreen) {
    if (clearScreen) {
        display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, DISPLAY_BLACK);
    }

    u8g2Fonts.setFont(FONT_MEDIUM);
    const char *warning = "Hold to Restart!";
    int16_t w = u8g2Fonts.getUTF8Width(warning);
    u8g2Fonts.setCursor((SCREEN_WIDTH - w) / 2, 24);
    u8g2Fonts.print(warning);

    drawProgressBar(display, 14, 42, 100, 8, progressPercent, true);
}

void UIComponents::drawOtaProgress(DisplayType& display, int percent) {
    // Clear entire screen for clean OTA mode
    display.clearDisplay();

    // Vertically centered progress bar
    int16_t barW = 100;
    int16_t barH = 8;
    int16_t barX = (SCREEN_WIDTH - barW) / 2;
    int16_t barY = (SCREEN_HEIGHT - barH) / 2 - 6; // slightly above center

    // Progress bar outline
    display.drawRoundRect(barX, barY, barW, barH, 3, DISPLAY_WHITE);

    // Progress bar fill
    int16_t fillW = (int16_t)((barW - 4) * percent / 100);
    if (fillW > 0) {
        display.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, DISPLAY_WHITE);
    }

    // Label below progress bar
    char label[32];
    snprintf(label, sizeof(label), "Updating: %d%%", percent);
    u8g2Fonts.setFont(FONT_MEDIUM);
    int16_t lbw = u8g2Fonts.getUTF8Width(label);
    u8g2Fonts.setCursor((SCREEN_WIDTH - lbw) / 2, barY + barH + 16);
    u8g2Fonts.print(label);
    display.display();
}

void UIComponents::showSleepScreen(DisplayType& display) {
    display.clearDisplay();
    display.fillCircle(64, 24, 10, DISPLAY_WHITE);
    display.fillCircle(58, 20, 10, DISPLAY_BLACK); // Crescent cutout

    u8g2Fonts.setFont(FONT_MEDIUM);
    const char *sleepMsg = "Sleeping...";
    int16_t sw = u8g2Fonts.getUTF8Width(sleepMsg);
    u8g2Fonts.setCursor((SCREEN_WIDTH - sw) / 2, 52);
    u8g2Fonts.print(sleepMsg);

    u8g2Fonts.setFont(FONT_SMALL);
    const char *hintMsg = "Touch to wake";
    int16_t hw = u8g2Fonts.getUTF8Width(hintMsg);
    u8g2Fonts.setCursor((SCREEN_WIDTH - hw) / 2, 63);
    u8g2Fonts.print(hintMsg);

    display.display();
    delay(2000); // Let user see the message

    display.clearDisplay();
    display.display();
}

void UIComponents::showLowBatteryScreen(DisplayType& display, uint16_t holdDelayMs) {
    display.clearDisplay();
    u8g2Fonts.setFont(FONT_MEDIUM);
    const char *msg = "LOW BATTERY";
    int16_t mw = u8g2Fonts.getUTF8Width(msg);
    u8g2Fonts.setCursor((SCREEN_WIDTH - mw) / 2, 36);
    u8g2Fonts.print(msg);
    display.display();
    
    delay(holdDelayMs);
    
    display.clearDisplay();
    display.display();
}

void UIComponents::showChargingAnimation(DisplayType& display, uint8_t batteryPercent) {
    int solidBars = 0;
    if      (batteryPercent >= 80) solidBars = 3;
    else if (batteryPercent >= 55) solidBars = 2;
    else if (batteryPercent >= 30) solidBars = 1;

    const int16_t bx = (SCREEN_WIDTH - 44) / 2;
    const int16_t by = 36;
    const int16_t barOffsets[] = {2, 11, 20, 29};

    // Show for ~3 seconds with blinking bar animation
    for (int frame = 0; frame < 30; frame++) {  // 30 frames × 100ms = 3s
        display.clearDisplay();

        // "Charging" title
        u8g2Fonts.setFont(FONT_MEDIUM);
        const char* title = "Charging";
        int16_t tw = u8g2Fonts.getUTF8Width(title);
        u8g2Fonts.setCursor((SCREEN_WIDTH - tw) / 2, 25);
        u8g2Fonts.print(title);

        // Battery body (40x16) + nub
        display.drawRect(bx, by, 40, 16, DISPLAY_WHITE);
        display.fillRect(bx + 40, by + 4, 3, 8, DISPLAY_WHITE);

        bool blinkOn = (frame / 5) % 2 == 0;  // Toggle every 500ms

        for (int i = 0; i < 4; i++) {
            if (i < solidBars) {
                display.fillRect(bx + barOffsets[i], by + 2, 7, 12, DISPLAY_WHITE);
            } else if (i == solidBars) {
                if (blinkOn) display.fillRect(bx + barOffsets[i], by + 2, 7, 12, DISPLAY_WHITE);
            }
        }

        display.display();
        delay(100);
    }
}
