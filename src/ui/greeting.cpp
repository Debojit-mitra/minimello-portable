#include "greeting.h"
#include "display_config.h"
#include "font_config.h"
#include "config.h"

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <math.h>

extern U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// =============================================================
// Greeting Animation — Implementation
// =============================================================

static Emotion getRestingEmotion(int hour) {
    if (hour < 0) return Emotion::NEUTRAL;
    bool isNight = (hour >= 23 || hour < 7);
    if (isNight) return Emotion::SLEEPY;
    if (hour >= 6 && hour < 9) return Emotion::HAPPY;
    return Emotion::NEUTRAL;
}

// Shift the display buffer UP by `pixels` rows.
static void shiftBufferUp(DisplayType& d, uint8_t pixels) {
    if (pixels == 0) return;
    uint8_t* buf = d.getBuffer();
    int16_t w = d.width();
    int16_t pages = d.height() / 8;
    uint8_t pageShift = pixels / 8;
    uint8_t bitShift  = pixels % 8;

    for (int16_t col = 0; col < w; col++) {
        for (int16_t page = 0; page < pages; page++) {
            int16_t srcPage = page + pageShift;
            uint8_t cur  = (srcPage < pages)     ? buf[srcPage * w + col]       : 0;
            uint8_t next = (srcPage + 1 < pages) ? buf[(srcPage + 1) * w + col] : 0;
            buf[page * w + col] = (cur >> bitShift) | (next << (8 - bitShift));
        }
    }
}

// Shift the display buffer LEFT by `pixels` columns.
static void shiftBufferLeft(DisplayType& d, uint8_t pixels) {
    if (pixels == 0) return;
    uint8_t* buf = d.getBuffer();
    int16_t w = d.width();
    int16_t pages = d.height() / 8;

    for (int16_t page = 0; page < pages; page++) {
        uint8_t* row = buf + page * w;
        // Shift columns left
        for (int16_t x = 0; x < w; x++) {
            int16_t src = x + pixels;
            row[x] = (src < w) ? row[src] : 0;
        }
    }
}

// --- Waving hand ---
static void drawWavingHand(DisplayType& d, int16_t cx, int16_t cy, float waveAngle) {
    float rad = waveAngle * 3.14159f / 180.0f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    int16_t wx = cx;
    int16_t wy = cy;

    auto drawRotatedCircle = [&](float x, float y, int16_t r) {
        int16_t rx = wx + (int16_t)(x * cosA - y * sinA);
        int16_t ry = wy + (int16_t)(x * sinA + y * cosA);
        d.fillCircle(rx, ry, r, DISPLAY_WHITE);
    };

    auto drawThickFinger = [&](float x1, float y1, float x2, float y2, int16_t r) {
        // Draw 4 overlapping circles for a very smooth pill shape
        for (int i = 0; i <= 3; i++) {
            float t = i / 3.0f;
            drawRotatedCircle(x1 + (x2 - x1) * t, y1 + (y2 - y1) * t, r);
        }
    };

    // Palm - single solid circle for a smooth, cohesive shape
    drawRotatedCircle(0, -6, 6);
    
    // Fill bottom edge (wrist area) slightly to square it off a bit
    drawRotatedCircle(0, -2, 4);

    // Thumb (left side, splays left/down)
    drawThickFinger(-5, -4, -10, -1, 2);

    // 3 distinct fingers with slight gaps (r=2 means 4px thick)
    // Splaying outward for a natural look
    drawThickFinger(-5, -9,  -6, -16, 2);  // Index
    drawThickFinger( 0, -10,  0, -18, 2);  // Middle
    drawThickFinger( 5, -9,   6, -16, 2);  // Ring/Pinky
}

void playGreeting(DisplayType& display, EmotionEngine& emotionEngine, const String& userName, int hour) {
    // --- Build greeting text ---
    const char* greeting;
    if (hour < 0) {
        greeting = "Hey there";
    } else if (hour >= 5 && hour < 12) {
        greeting = "Good Morning";
    } else if (hour >= 12 && hour < 17) {
        greeting = "Good Afternoon";
    } else if (hour >= 17 && hour < 21) {
        greeting = "Good Evening";
    } else {
        greeting = "Hey there";
    }

    bool hasName = userName.length() > 0;
    String greetText;
    if (hasName) {
        greetText = String(greeting) + ", " + userName + "!";
    } else {
        greetText = "Hello! I'm MiniMello!";
    }

    // --- Start at NEUTRAL ---
    emotionEngine.setEmotion(Emotion::NEUTRAL);
    for (int i = 0; i < 10; i++) {
        emotionEngine.update(20);
    }

    // --- Timing ---
    const uint32_t slideInEndMs   = 500;
    const uint32_t holdEndMs      = 2400;
    const uint32_t slideOutEndMs  = 2900;
    const uint32_t totalMs        = 3400;

    const uint8_t  faceShiftUp    = 10;   // pixels to shift face up
    const uint8_t  faceShiftLeft  = 16;   // pixels to shift face left (room for hand)
    const int16_t  textSlideH     = 14;

    // Hand position (right side of screen after face shifts left)
    const int16_t  handX = 110;
    const int16_t  handY = 32;

    Emotion restingEmotion = getRestingEmotion(hour);
    bool triggeredHappy = false;
    bool triggeredBack  = false;

    uint32_t startMs = millis();
    uint32_t lastFrameMs = startMs;

    while (true) {
        uint32_t now = millis();
        uint32_t elapsed = now - startMs;
        if (elapsed >= totalMs) break;

        uint32_t deltaMs = now - lastFrameMs;
        lastFrameMs = now;
        if (deltaMs > 100) deltaMs = 100;

        // --- Emotion transitions ---
        if (!triggeredHappy && elapsed >= 200) {
            emotionEngine.setEmotion(Emotion::HAPPY);
            triggeredHappy = true;
        }
        if (!triggeredBack && elapsed >= holdEndMs) {
            emotionEngine.setEmotion(restingEmotion);
            triggeredBack = true;
        }

        emotionEngine.update(deltaMs);

        // --- Render face at normal position ---
        display.clearDisplay();
        emotionEngine.render(display);

        // --- Animation progress (0 → 1 → 1 → 0) ---
        float progress = 0.0f;
        if (elapsed < slideInEndMs) {
            float t = (float)elapsed / slideInEndMs;
            progress = 1.0f - (1.0f - t) * (1.0f - t);  // ease-out
        } else if (elapsed < holdEndMs) {
            progress = 1.0f;
        } else if (elapsed < slideOutEndMs) {
            float t = (float)(elapsed - holdEndMs) / (slideOutEndMs - holdEndMs);
            progress = 1.0f - t * t;  // ease-in reverse
        }

        // --- Shift face: up + left ---
        uint8_t shiftUp   = (uint8_t)(faceShiftUp * progress);
        uint8_t shiftLeft = (uint8_t)(faceShiftLeft * progress);
        shiftBufferUp(display, shiftUp);
        shiftBufferLeft(display, shiftLeft);

        // --- Clear right edge (behind hand area) ---
        if (progress > 0.05f) {
            int16_t handAreaX = SCREEN_WIDTH - (int16_t)(faceShiftLeft * progress) - 2;
            if (handAreaX < SCREEN_WIDTH - 4) {
                display.fillRect(handAreaX, 0, SCREEN_WIDTH - handAreaX, 50, DISPLAY_BLACK);
            }
        }

        // --- Draw waving hand ---
        if (progress > 0.3f) {
            float waveT = (float)elapsed / 1000.0f;
            
            // Waving rotation (peaks at 25 degrees)
            float waveAngle = 25.0f * sinf(waveT * 10.0f) * progress;
            
            // Add a slight U-shape spatial translation to the wrist pivot 
            // for a natural, organic "wiping" motion.
            int16_t bounceY = (int16_t)(-2.0f * cosf(waveT * 20.0f) * progress);
            int16_t swayX   = (int16_t)(1.5f * sinf(waveT * 10.0f) * progress);

            int16_t hx = handX - (int16_t)(faceShiftLeft * (1.0f - progress)) + swayX;
            drawWavingHand(display, hx, handY + bounceY, waveAngle);
        }

        // --- Clear bottom strip + draw text ---
        display.fillRect(0, 52, SCREEN_WIDTH, 12, DISPLAY_BLACK);

        int16_t textOffY = (int16_t)(textSlideH * (1.0f - progress));
        if (progress > 0.05f) {
            int16_t drawY = 62 + textOffY;

            u8g2Fonts.setFontMode(1);
            u8g2Fonts.setForegroundColor(DISPLAY_WHITE);
            u8g2Fonts.setFont(FONT_MEDIUM);

            const char* showText = greetText.c_str();
            int16_t tw = u8g2Fonts.getUTF8Width(showText);
            u8g2Fonts.setCursor((SCREEN_WIDTH - tw) / 2, drawY);
            u8g2Fonts.print(showText);

            if (!hasName && elapsed > slideInEndMs && elapsed < holdEndMs) {
                u8g2Fonts.setFont(FONT_SMALL);
                const char* hint = "Set name at minimello.local";
                int16_t hw = u8g2Fonts.getUTF8Width(hint);
                u8g2Fonts.setCursor((SCREEN_WIDTH - hw) / 2, drawY - 12);
                u8g2Fonts.print(hint);
            }
        }

        display.display();
        delay(16);
    }
}
