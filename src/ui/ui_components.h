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

    // Draws a full-screen OTA update progress bar (clears screen)
    static void drawOtaProgress(DisplayType& display, int percent);

    // Shows a transient sleep notification ("Sleeping...", crescent moon) and delays
    static void showSleepScreen(DisplayType& display);

    // Shows a transient low battery warning ("LOW BATTERY") and delays
    static void showLowBatteryScreen(DisplayType& display, uint16_t holdDelayMs = 2000);

    // Plays a one-shot feature-phone style charging animation for ~3 seconds
    static void showChargingAnimation(DisplayType& display, uint8_t batteryPercent);
};
