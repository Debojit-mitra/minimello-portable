#pragma once

#include <U8g2_for_Adafruit_GFX.h>

// =============================================================
// Minimello Portable — Font Configuration (U8g2)
// =============================================================
// Smooth bitmap fonts for monochrome OLED via U8g2_for_Adafruit_GFX.
// These replace the default blocky 5x7 Adafruit GFX font.
//
// Font naming: _tf = transparent full charset, _tr = transparent reduced
// =============================================================

// --- Font Aliases ---
// Small: compact, for status bar text, boot labels, humidity
#define FONT_SMALL    u8g2_font_profont10_tf

// Medium: clean Helvetica-like bold, for date lines, city names, descriptions
#define FONT_MEDIUM   u8g2_font_helvB08_tf

// Large: bold Helvetica for city name on weather screen
#define FONT_LARGE    u8g2_font_helvB10_tf

// Large Temp: bold Helvetica 12px for weather temperature
#define FONT_LARGE_TEMP u8g2_font_helvB12_tf

// Large number: bold Helvetica, for analog face day number
#define FONT_LARGE_NUM u8g2_font_helvB14_tf

// Extra Large: 16px clean sans-serif for weather temperature
#define FONT_WEATHER_TEMP u8g2_font_logisoso16_tf

// --- Global U8g2 font renderer (defined in main.cpp) ---
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
