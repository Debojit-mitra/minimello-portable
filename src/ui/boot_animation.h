#ifndef BOOT_ANIMATION_H
#define BOOT_ANIMATION_H

#include <Arduino.h>
#include "display_config.h"
#include "network/network_manager.h"
#include "server/web_server.h"
#include "touch/touch_manager.h"

// Plays the initial startup animation (spring physics on logo)
void playBootAnimation();

// Draws a progress bar indicating boot stages (blocking)
void drawBootProgress(uint8_t step, uint8_t totalSteps, const char* label);

// Blocks and renders the setup screen while in AP mode
void runSetupScreen(DisplayType& display, NetworkManager& networkMgr, MiniWebServer& webServer, TouchManager& touchMgr);

#endif // BOOT_ANIMATION_H
