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
