#include "emotion_engine.h"
#include "config.h"
#include "clock/clock_engine.h"
#include "bitmaps/icons.h"
#include <math.h>

void EmotionEngine::render(DisplayType& display) {
    int16_t yOff = (int16_t)(_current.bounce + _idleBounce);

    // Draw face elements (back to front)
    drawBlush(display, yOff);
    drawEyebrow(display, true, yOff);    // Left brow
    drawEyebrow(display, false, yOff);   // Right brow

    // Eye rendering: choose mode based on current params
    if (_current.heartEyes) {
        drawHeart(display, (int16_t)_current.eyeLX, (int16_t)_current.eyeLY + yOff,
                  (int16_t)(_current.eyeW * 1.8f));
        drawHeart(display, (int16_t)_current.eyeRX, (int16_t)_current.eyeRY + yOff,
                  (int16_t)(_current.eyeW * 1.8f));
    } else if (_current.arcEyes) {
        drawArcEye(display, true, yOff);
        drawArcEye(display, false, yOff);
    } else if (_current.winkLeft) {
        drawArcEye(display, true, yOff);   // Left eye = closed arc (wink)
        drawEye(display, false, yOff);      // Right eye = normal
    } else {
        drawEye(display, true, yOff);
        drawEye(display, false, yOff);
    }

    drawMouth(display, yOff);
    drawParticles(display);
}

// --- Public API ---


void EmotionEngine::drawEye(DisplayType& d, bool isLeft, int16_t yOff) {
    float cx = isLeft ? _current.eyeLX : _current.eyeRX;
    float cy = (isLeft ? _current.eyeLY : _current.eyeRY) + yOff;
    float w = _current.eyeW;
    float h = _current.eyeH;

    // Apply blink: squish eye height
    if (_blinkAmount > 0) {
        h = h * (1.0f - _blinkAmount * 0.95f);
        if (h < 1) h = 1;
    }

    int16_t ix = (int16_t)cx;
    int16_t iy = (int16_t)cy;
    int16_t iw = (int16_t)(w * 2);
    int16_t ih = (int16_t)(h * 2);

    // Draw eye as filled rounded rectangle (sclera = white)
    int16_t r = min(iw, ih) / 3;
    if (r < 2) r = 2;
    d.fillRoundRect(ix - iw / 2, iy - ih / 2, iw, ih, r, DISPLAY_WHITE);

    // Draw pupil (black circle inside white eye)
    if (h > 3) {  // Don't draw pupil when eye is nearly closed
        float px = cx + _current.pupilOX + _idlePupilX;
        float py = cy + _current.pupilOY + _idlePupilY;
        int16_t pr = (int16_t)_current.pupilR;

        // Clamp pupil inside eye bounds
        if (px - pr < cx - w + 2) px = cx - w + 2 + pr;
        if (px + pr > cx + w - 2) px = cx + w - 2 - pr;
        if (py - pr < cy - h + 2) py = cy - h + 2 + pr;
        if (py + pr > cy + h - 2) py = cy + h - 2 - pr;

        d.fillCircle((int16_t)px, (int16_t)py, pr, DISPLAY_BLACK);

        // Highlight dot (gives eye a sparkle)
        if (pr >= 3) {
            d.drawPixel((int16_t)px - 1, (int16_t)py - 1, DISPLAY_WHITE);
        }
    }
}

void EmotionEngine::drawArcEye(DisplayType& d, bool isLeft, int16_t yOff) {
    // Draws a ^_^ style happy/wink closed eye as a curved arc
    float cx = isLeft ? _current.eyeLX : _current.eyeRX;
    float cy = (isLeft ? _current.eyeLY : _current.eyeRY) + yOff;
    float w = _current.eyeW;

    int16_t ix = (int16_t)cx;
    int16_t iy = (int16_t)cy;
    int16_t hw = (int16_t)w;  // half-width of the arc

    // Draw the arc as a smooth curve using segments
    // The arc goes from left to right, peaking upward in the middle
    // This creates the ^  shape of happy/closed eyes
    const int segments = 8;
    int16_t prevX = ix - hw;
    int16_t prevY = iy;

    for (int i = 1; i <= segments; i++) {
        float t = (float)i / segments;
        int16_t x = ix - hw + (int16_t)(2.0f * hw * t);
        // Arc curves upward: sin curve that peaks at -6 in the middle
        float arcHeight = -6.0f * sinf(t * 3.14159f);
        int16_t y = iy + (int16_t)arcHeight;

        // Draw thick line (3 pixels for visibility)
        d.drawLine(prevX, prevY, x, y, DISPLAY_WHITE);
        d.drawLine(prevX, prevY - 1, x, y - 1, DISPLAY_WHITE);
        d.drawLine(prevX, prevY + 1, x, y + 1, DISPLAY_WHITE);

        prevX = x;
        prevY = y;
    }
}

void EmotionEngine::drawHeart(DisplayType& d, int16_t cx, int16_t cy, int16_t size) {
    int16_t r = size / 3;
    if (r < 2) r = 2;

    // Two circles for the top bumps
    d.fillCircle(cx - r + 1, cy - r / 2, r, DISPLAY_WHITE);
    d.fillCircle(cx + r - 1, cy - r / 2, r, DISPLAY_WHITE);

    // Triangle for the bottom point
    d.fillTriangle(
        cx - size / 2 - 1, cy,
        cx + size / 2 + 1, cy,
        cx, cy + size / 2 + r / 2,
        DISPLAY_WHITE
    );
}

void EmotionEngine::drawEyebrow(DisplayType& d, bool isLeft, int16_t yOff) {
    if (!_current.browVisible) return;

    float cx = isLeft ? _current.eyeLX : _current.eyeRX;
    float cy = (isLeft ? _current.eyeLY : _current.eyeRY) + yOff;
    float angle = isLeft ? _current.browLAngle : _current.browRAngle;
    float halfLen = _current.browLen / 2.0f;

    // Brow sits above the eye
    float browCY = cy - _current.eyeH - _current.browOffY;

    // Inner and outer points with angle tilt
    float innerX, outerX;
    if (isLeft) {
        innerX = cx + halfLen;   // Toward nose
        outerX = cx - halfLen;   // Toward edge
    } else {
        innerX = cx - halfLen;
        outerX = cx + halfLen;
    }

    float innerY = browCY - angle;
    float outerY = browCY + angle;

    // Draw thick brow (3 pixels wide for visibility on 128x64)
    for (int i = -1; i <= 1; i++) {
        d.drawLine((int16_t)innerX, (int16_t)(innerY + i),
                   (int16_t)outerX, (int16_t)(outerY + i), DISPLAY_WHITE);
    }
}

void EmotionEngine::drawMouth(DisplayType& d, int16_t yOff) {
    int16_t cx = 64;
    int16_t cy = (int16_t)_current.mouthY + yOff;
    int16_t halfW = (int16_t)_current.mouthW;

    if (_current.mouthOpenH > 1.5f) {
        // Open mouth — draw as filled white oval with black interior
        int16_t openH = (int16_t)_current.mouthOpenH;
        int16_t r = min(halfW, (int16_t)(openH / 2));
        if (r < 2) r = 2;

        // White outline
        d.fillRoundRect(cx - halfW, cy - openH / 2,
                        halfW * 2, openH, r, DISPLAY_WHITE);
        // Black interior (makes it look like an open mouth)
        if (halfW > 3 && openH > 4) {
            d.fillRoundRect(cx - halfW + 2, cy - openH / 2 + 2,
                            halfW * 2 - 4, openH - 4, r - 1, DISPLAY_BLACK);
        }
    } else {
        // Closed mouth — curved arc using segments
        // IMPORTANT: positive mouthCurve = smile = curve goes DOWN (higher Y on screen)
        // This is correct because on screen, Y increases downward, so a ∪ shape = smile
        float curve = _current.mouthCurve;
        int16_t leftX = cx - halfW;
        int16_t rightX = cx + halfW;

        // Draw the smile/frown as a smooth arc
        const int segments = 8;
        int16_t prevX = leftX;
        int16_t prevY = cy;

        for (int i = 1; i <= segments; i++) {
            float t = (float)i / segments;
            int16_t x = leftX + (int16_t)((rightX - leftX) * t);
            // Sin curve: peaks at middle, amount = curve value
            // Positive curve → positive Y offset → lower on screen → ∪ = smile
            float offset = curve * sinf(t * 3.14159f);
            int16_t y = cy + (int16_t)offset;

            // Draw thick line (2-3 pixels for visibility)
            d.drawLine(prevX, prevY, x, y, DISPLAY_WHITE);
            d.drawLine(prevX, prevY + 1, x, y + 1, DISPLAY_WHITE);

            // Extra thickness for big smiles/frowns
            if (fabsf(curve) >= 6) {
                d.drawLine(prevX, prevY - 1, x, y - 1, DISPLAY_WHITE);
            }

            prevX = x;
            prevY = y;
        }
    }
}

void EmotionEngine::drawBlush(DisplayType& d, int16_t yOff) {
    if (_current.blushR < 1) return;

    int16_t r = (int16_t)_current.blushR;

    // Position blush below and to the outside of each eye
    int16_t ly = (int16_t)_current.eyeLY + yOff + (int16_t)_current.eyeH + 3;
    int16_t ry = (int16_t)_current.eyeRY + yOff + (int16_t)_current.eyeH + 3;
    int16_t lx = (int16_t)_current.eyeLX - 6;
    int16_t rx = (int16_t)_current.eyeRX + 6;

    // Draw blush as small filled circles with horizontal lines pattern
    // This creates a cute striped blush effect visible at low resolution
    for (int16_t dy = -r; dy <= r; dy += 2) {
        int16_t halfW = (int16_t)sqrtf((float)(r * r - dy * dy));
        d.drawFastHLine(lx - halfW, ly + dy, halfW * 2, DISPLAY_WHITE);
        d.drawFastHLine(rx - halfW, ry + dy, halfW * 2, DISPLAY_WHITE);
    }
}

void EmotionEngine::drawParticles(DisplayType& d) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!_particles[i].active) continue;

        int16_t px = (int16_t)_particles[i].x;
        int16_t py = (int16_t)_particles[i].y;

        // Scale based on remaining life (fade out by shrinking)
        float scale = min(1.0f, _particles[i].life);

        switch (_particles[i].type) {
            case 0: // Heart
                if (scale > 0.3f) {
                    d.drawBitmap(px - 4, py - 4, sprite_heart, 8, 8, DISPLAY_WHITE);
                } else {
                    d.drawPixel(px, py, DISPLAY_WHITE);
                }
                break;
            case 1: // Star
                if (scale > 0.3f) {
                    d.drawBitmap(px - 4, py - 4, sprite_star, 8, 8, DISPLAY_WHITE);
                } else {
                    d.drawPixel(px, py, DISPLAY_WHITE);
                }
                break;
            case 2: // Zzz
                if (scale > 0.5f) {
                    d.drawBitmap(px - 4, py - 4, sprite_zzz, 8, 8, DISPLAY_WHITE);
                } else {
                    d.setTextSize(1);
                    d.setTextColor(DISPLAY_WHITE);
                    d.setCursor(px, py);
                    d.print('z');
                }
                break;
        }
    }
}
