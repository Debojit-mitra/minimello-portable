#include "clock_engine.h"
#include "bitmaps/icons.h"
#include "font_config.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <math.h>

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

    int16_t startX = 18 + _pixelShiftX; 
    
    // Mathematically perfect optical baselines
    int16_t y1 = 11 + _pixelShiftY;
    int16_t y2 = 28 + _pixelShiftY;
    int16_t y3 = 41 + _pixelShiftY;
    int16_t y4 = 60 + _pixelShiftY;
    
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



