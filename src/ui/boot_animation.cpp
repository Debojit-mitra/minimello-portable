#include "boot_animation.h"
#include "display_config.h"
#include "font_config.h"
#include "ui_components.h"
#include "version.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <U8g2_for_Adafruit_GFX.h>

extern DisplayType display;
extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void drawScaledCanvas(GFXcanvas1 *canvas, int16_t cx, int16_t cy, float scale) {
  if (!canvas)
    return;
  int16_t cw = canvas->width();
  int16_t ch = canvas->height();
  int16_t sw = cw * scale;
  int16_t sh = ch * scale;
  if (sw <= 0 || sh <= 0)
    return;

  int16_t startX = cx - sw / 2;
  int16_t startY = cy - sh / 2;
  uint8_t *buffer = canvas->getBuffer();
  uint16_t bytesPerRow = (cw + 7) / 8;

  for (int16_t i = 0; i < sw; i++) {
    for (int16_t j = 0; j < sh; j++) {
      int16_t origX = i / scale;
      int16_t origY = j / scale;
      if (origX < cw && origY < ch) {
        uint8_t byte = buffer[origY * bytesPerRow + origX / 8];
        if (byte & (0x80 >> (origX % 8))) {
          display.drawPixel(startX + i, startY + j, DISPLAY_WHITE);
        }
      }
    }
  }
}

void playBootAnimation() {
  const char *text = FIRMWARE_NAME;
  uint8_t len = strlen(text);

  display.setFont(&FreeSansBold12pt7b);
  display.setTextSize(1);
  display.setTextColor(DISPLAY_WHITE);

  // Calculate kerning and total advance
  int16_t charX[16];
  display.setCursor(0, 0);
  for (uint8_t i = 0; i < len; i++) {
    charX[i] = display.getCursorX();
    display.print(text[i]);
  }
  int16_t totalAdvance = display.getCursorX();
  int16_t startScreenX = (SCREEN_WIDTH - totalAdvance) / 2;

  // Pre-render canvases
  GFXcanvas1 *canvases[16] = {nullptr};
  int16_t cx[16] = {0};
  int16_t cyOffset[16] = {0};

  for (uint8_t i = 0; i < len; i++) {
    int16_t bx, by;
    uint16_t bw, bh;
    char buf[2] = {text[i], 0};
    display.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);

    if (bw > 0 && bh > 0) {
      canvases[i] = new GFXcanvas1(bw, bh);
      canvases[i]->setFont(&FreeSansBold12pt7b);
      canvases[i]->setTextSize(1);
      canvases[i]->setTextColor(1);
      canvases[i]->setCursor(-bx, -by);
      canvases[i]->print(buf);

      cx[i] = startScreenX + charX[i] + bx + bw / 2;
      cyOffset[i] = by + bh / 2;
    }
  }

  float baseY = 38; // Perfectly centered vertically for the initial pop
  float scales[16] = {0};
  float velocities[16] = {0};
  bool animating[16] = {false};

  float tension = 0.5; // Tuned for noticeable bounce
  float friction = 0.55;

  // Format version text early so we know its width
  char verBuf[16];
  snprintf(verBuf, sizeof(verBuf), "v%s", FIRMWARE_VERSION);
  u8g2Fonts.setFont(FONT_MEDIUM);
  int16_t vbw = u8g2Fonts.getUTF8Width(verBuf);

  uint32_t startTime = millis();
  uint8_t nextCharToStart = 0;
  uint32_t staggerMs = 85; // animation speed/duration

  // Overlapping animation loop
  while (true) {
    uint32_t now = millis();

    // Trigger the next character in sequence if its time has arrived
    if (nextCharToStart < len &&
        (now - startTime) >= (nextCharToStart * staggerMs)) {
      animating[nextCharToStart] = true;
      scales[nextCharToStart] = 0.1;
      velocities[nextCharToStart] = 0.2; // Initial pop boost
      nextCharToStart++;
    }

    bool anyAnimating = false;
    display.clearDisplay();

    // Update physics and draw for all characters
    for (uint8_t i = 0; i < len; i++) {
      if (animating[i]) {
        float delta = 1.0 - scales[i];
        velocities[i] += delta * 0.45; // Slightly lower tension for slower pop
        velocities[i] *= 0.55;         // Same friction
        scales[i] += velocities[i];

        if (abs(1.0 - scales[i]) < 0.01 && abs(velocities[i]) < 0.01) {
          scales[i] = 1.0;
          animating[i] = false;
        } else {
          anyAnimating = true;
        }
      }

      if (scales[i] > 0) {
        drawScaledCanvas(canvases[i], cx[i], baseY + cyOffset[i], scales[i]);
      }
    }
    display.display();

    // Exit loop when all characters have spawned and finished their spring
    // animations
    if (nextCharToStart >= len && !anyAnimating) {
      break;
    }

    delay(10); // Maintain a stable frame rate without blocking heavily
  }

  // Full logo display with version appearing
  display.clearDisplay();
  for (uint8_t i = 0; i < len; i++) {
    drawScaledCanvas(canvases[i], cx[i], baseY + cyOffset[i], 1.0);
  }
  u8g2Fonts.setCursor(
      (SCREEN_WIDTH - vbw) / 2,
      baseY + 14); // Offset so it lands exactly at y=38 when sliding to 24
  u8g2Fonts.print(verBuf);
  display.display();

  delay(800); // Pause to admire

  // Slide UP animation
  float targetY = 24; // Slide up to make room for progress bar
  float yVel = 0;
  while (abs(targetY - baseY) > 0.5 || abs(yVel) > 0.1) {
    float delta = targetY - baseY;
    yVel += delta * 0.25; // Faster slide
    yVel *= 0.6;
    baseY += yVel;

    display.clearDisplay();
    for (uint8_t i = 0; i < len; i++) {
      drawScaledCanvas(canvases[i], cx[i], baseY + cyOffset[i], 1.0);
    }
    u8g2Fonts.setCursor((SCREEN_WIDTH - vbw) / 2, baseY + 14);
    u8g2Fonts.print(verBuf);
    display.display();
    delay(8);
  }

  // Clean up
  for (uint8_t i = 0; i < len; i++) {
    if (canvases[i])
      delete canvases[i];
  }
}

void drawBootProgress(uint8_t step, uint8_t totalSteps, const char *label) {


  // Progress bar area
  int16_t barX = 14;
  int16_t barY = 42;
  int16_t barW = 100;
  int16_t barH = 8;

  // Clear progress area only (keep text above y=40, preserving the version text
  // which ends at y=38)
  display.fillRect(0, 40, SCREEN_WIDTH, 24, DISPLAY_BLACK);

  // Draw shared progress bar
  uint8_t progressPercent = (uint8_t)(100 * step / totalSteps);
  UIComponents::drawProgressBar(display, barX, barY, barW, barH,
                                progressPercent, true);

  // Label below progress bar
  u8g2Fonts.setFont(FONT_MEDIUM);
  int16_t lbw = u8g2Fonts.getUTF8Width(label);
  u8g2Fonts.setCursor((SCREEN_WIDTH - lbw) / 2, 62);
  u8g2Fonts.print(label);

  display.display();
}

void runSetupScreen(DisplayType &display, NetworkManager &networkMgr,
                    MiniWebServer &webServer, TouchManager &touchMgr) {
  uint32_t lastToggleMs = 0;
  uint8_t toggleState = 0;
  bool isShowingRestartWarning = false;

  // Block here as long as we are in AP mode and NOT connected
  while (networkMgr.getState() == NetState::AP_MODE &&
         !networkMgr.isConnected()) {
    uint32_t now = millis();

    // Must keep network, web server, and touch alive!
    networkMgr.update(now);
    webServer.update();
    touchMgr.update();

    uint32_t holdTime = touchMgr.getHoldTimeMs();

    if (holdTime > 3000) {
      isShowingRestartWarning = true;
      uint8_t pct = (holdTime >= 5000) ? 100 : ((holdTime - 3000) * 100 / 2000);
      UIComponents::drawRestartWarning(display, pct, true);
      display.display();

      // touchMgr.update() will fire TouchEvent::VERY_LONG_PRESS which is caught
      // by main.cpp
    } else {
      // If we just released from a warning, force an immediate redraw of the
      // setup text
      if (isShowingRestartWarning) {
        isShowingRestartWarning = false;
        lastToggleMs = 0; // Force immediate redraw

        // Redraw basic setup header since we cleared the screen
        display.clearDisplay();
        u8g2Fonts.setFont(FONT_MEDIUM);
        int16_t vbw = u8g2Fonts.getUTF8Width("v" FIRMWARE_VERSION);
        u8g2Fonts.setCursor((SCREEN_WIDTH - vbw) / 2, 24 + 14);
        u8g2Fonts.print("v" FIRMWARE_VERSION);

        int16_t tw = u8g2Fonts.getUTF8Width("Setup Mode");
        u8g2Fonts.setCursor((SCREEN_WIDTH - tw) / 2, 24);
        u8g2Fonts.print("Setup Mode");
      }

      // Toggle text every 3 seconds for readability
      if (now - lastToggleMs >= 3000) {
        lastToggleMs = now;

        // Clear the bottom half of the screen
        display.fillRect(0, 40, SCREEN_WIDTH, 24, DISPLAY_BLACK);
        u8g2Fonts.setFont(FONT_MEDIUM);

        String text1, text2;
        if (toggleState == 0) {
          text1 = "Setup Required";
          text2 = "Connect to WiFi";
        } else if (toggleState == 1) {
          text1 = networkMgr.getAPName();
          text2 = "pw: " WIFI_AP_PASSWORD;
        } else {
          text1 = "Open Browser:";
          text2 = networkMgr.getIP();
        }

        int16_t w1 = u8g2Fonts.getUTF8Width(text1.c_str());
        int16_t w2 = u8g2Fonts.getUTF8Width(text2.c_str());

        u8g2Fonts.setCursor((SCREEN_WIDTH - w1) / 2, 50);
        u8g2Fonts.print(text1.c_str());

        u8g2Fonts.setCursor((SCREEN_WIDTH - w2) / 2, 64);
        u8g2Fonts.print(text2.c_str());

        display.display();

        toggleState = (toggleState + 1) % 3;
      }
    }

    yield(); // Feed watchdog
  }
}
