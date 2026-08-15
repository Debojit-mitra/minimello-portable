#pragma once

#include "config.h"
#include <Wire.h>

#if USE_1_3_INCH_OLED
    #include <Adafruit_SH110X.h>
    typedef Adafruit_SH1106G DisplayType;
    #define DISPLAY_BLACK SH110X_BLACK
    #define DISPLAY_WHITE SH110X_WHITE
    #define DISPLAY_SETCONTRAST(d, val) d.setContrast(val)
    #define DISPLAY_BEGIN(d, addr, reset) d.begin(addr, true)
#else
    #include <Adafruit_SSD1306.h>
    typedef Adafruit_SSD1306 DisplayType;
    #define DISPLAY_BLACK SSD1306_BLACK
    #define DISPLAY_WHITE SSD1306_WHITE

    // Multi-parameter brightness for SSD1306.
    // To achieve smooth dimming without jarring jumps, we simultaneously
    // interpolate the Contrast register (using a gamma curve) and the
    // Precharge Phase 2 period.
    // VCOMH is kept at the default maximum (0x40) so that 100% brightness
    // matches the original full brightness.
    inline void _ssd1306SetBrightness(Adafruit_SSD1306& d, uint8_t level) {
        if (level == 0) {
            d.ssd1306_command(SSD1306_SETCONTRAST);
            d.ssd1306_command(0);
            return;
        }

        // Interpolate Precharge Phase 2 from 1 to 15. Phase 1 is kept at 1.
        // Default at full brightness is 0xF1 (Phase 2 = 15, Phase 1 = 1).
        uint8_t phase2 = 1 + ((level - 1) * 14) / 254;
        uint8_t precharge = (phase2 << 4) | 0x01;

        // Apply a gamma curve (2.0) to the contrast to give more resolution at low levels.
        // Cap maximum contrast at 207 (0xCF). Values higher than 0xCF on 128x64
        // internal charge pump displays cause voltage sag, which actually dims the screen.
        float normalized = level / 255.0f;
        float corrected = powf(normalized, 2.0f);
        uint8_t contrast = (uint8_t)(corrected * 207.0f + 0.5f);
        if (contrast == 0) contrast = 1;

        d.ssd1306_command(SSD1306_SETCONTRAST);
        d.ssd1306_command(contrast);
        d.ssd1306_command(SSD1306_SETPRECHARGE);
        d.ssd1306_command(precharge);
        d.ssd1306_command(SSD1306_SETVCOMDETECT);
        d.ssd1306_command(0x40); // Restore Adafruit default VCOMH
    }

    #define DISPLAY_SETCONTRAST(d, val) _ssd1306SetBrightness(d, val)
    #define DISPLAY_BEGIN(d, addr, reset) d.begin(SSD1306_SWITCHCAPVCC, addr, reset)
#endif


