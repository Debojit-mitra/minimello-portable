#pragma once

#include <Arduino.h>
#include "display_config.h"

// =============================================================
// UI Components
// =============================================================
// Reusable UI rendering utilities for standard visual elements.

class UIComponents {
public:
    // Draws a standard progress bar.
    // progressPercent: 0 to 100
    // rounded: true for rounded corners, false for square corners
    static void drawProgressBar(DisplayType& display, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t progressPercent, bool rounded = true);

    // Draws the "Hold to Restart!" warning text and progress bar.
    // Clears the screen first (if clearScreen is true).
    static void drawRestartWarning(DisplayType& display, uint8_t progressPercent, bool clearScreen = true);
};
