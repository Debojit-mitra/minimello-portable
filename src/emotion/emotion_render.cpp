#include "emotion_engine.h"
#include "config.h"
#include "clock/clock_engine.h"
#include "bitmaps/icons.h"
#include <math.h>

void EmotionEngine::render(DisplayType& display) {
    int16_t yOff = (int16_t)(_current.bounce + _idleBounce);

    // Draw face elements
    drawBlush(display, yOff);

    // Eye rendering: choose mode based on current params
    if (_current.heartEyes) {
        drawHeart(display, (int16_t)_current.eyeLX, (int16_t)_current.eyeLY + yOff,
                  (int16_t)(_current.eyeW * 1.8f));
        drawHeart(display, (int16_t)_current.eyeRX, (int16_t)_current.eyeRY + yOff,
                  (int16_t)(_current.eyeW * 1.8f));
    } else if (_current.squeezeEyes) {
        drawSqueezeEye(display, true, yOff);
        drawSqueezeEye(display, false, yOff);
    } else if (_current.arcEyes) {
        drawArcEye(display, true, yOff);
        drawArcEye(display, false, yOff);
    } else if (_current.winkLeft) {
        drawArcEye(display, true, yOff);   // Left eye = closed arc (wink)
        drawEye(display, false, yOff);      // Right eye = normal
    } else if (_current.winkRightSqueeze) {
        drawEye(display, true, yOff);             // Left eye = normal
        drawSqueezeEye(display, false, yOff);     // Right eye = closed squeeze (<)
    } else {
        drawEye(display, true, yOff);
        drawEye(display, false, yOff);
    }

    // Draw eyebrows AFTER eyes so they can mask out the top of the eye
    drawEyebrow(display, true, yOff);
    drawEyebrow(display, false, yOff);

    drawHappyMarks(display, yOff);

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
    // Using min(iw, ih) / 2.2 makes the eyes much rounder, closer to a pill/oval shape
    int16_t r = (int16_t)(min(iw, ih) / 2.2f);
    if (r < 2) r = 2;
    d.fillRoundRect(ix - iw / 2, iy - ih / 2, iw, ih, r, DISPLAY_WHITE);

    // Draw pupil (black oval inside white eye)
    if (h > 3) {  // Don't draw pupil when eye is nearly closed
        float px = cx + _current.pupilOX + _idlePupilX;
        float py = cy + _current.pupilOY + _idlePupilY;
        int16_t pr = (int16_t)(_current.pupilR * 1.8f); // More larger pupil
        int16_t pr_y = (int16_t)(pr * 1.15f); // Make it almost circular like the reference image

        // Clamp pupil inside eye bounds
        if (px - pr < cx - w + 2) px = cx - w + 2 + pr;
        if (px + pr > cx + w - 2) px = cx + w - 2 - pr;
        if (py - pr_y < cy - h + 2) py = cy - h + 2 + pr_y;
        if (py + pr_y > cy + h - 2) py = cy + h - 2 - pr_y;

        int16_t px_i = (int16_t)px;
        int16_t py_i = (int16_t)py;

        // Draw as a true ellipse to ensure smooth continuous curves (not a hexagonal pill)
        for (int16_t dy = -pr_y; dy <= pr_y; dy++) {
            float t = (float)dy / (float)pr_y;
            int16_t dx = (int16_t)(pr * sqrtf(1.0f - t * t) + 0.5f);
            if (dx > 0) { // Skip drawing single-pixel tips at the poles
                d.drawFastHLine(px_i - dx, py_i + dy, dx * 2 + 1, DISPLAY_BLACK);
            }
        }

        // Circular highlight inside each pupil, positioned in the upper right
        if (pr >= 3) {
            int16_t hl_r = max((int16_t)1, (int16_t)(pr * 0.40f)); // Larger highlight like the image
            d.fillCircle(px_i + pr / 2, py_i - pr_y / 2 + 1, hl_r, DISPLAY_WHITE);
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
        float squash = 1.0f - _blinkAmount * 0.95f;
        float arcHeight = -6.0f * sinf(t * 3.14159f) * squash;
        int16_t y = iy + (int16_t)arcHeight;

        // Draw thick line (3 pixels for visibility)
        d.drawLine(prevX, prevY, x, y, DISPLAY_WHITE);
        d.drawLine(prevX, prevY - 1, x, y - 1, DISPLAY_WHITE);
        d.drawLine(prevX, prevY + 1, x, y + 1, DISPLAY_WHITE);

        prevX = x;
        prevY = y;
    }
}

void EmotionEngine::drawSqueezeEye(DisplayType& d, bool isLeft, int16_t yOff) {
    // Draws a > or < style excited closed eye
    float cx = isLeft ? _current.eyeLX : _current.eyeRX;
    float cy = (isLeft ? _current.eyeLY : _current.eyeRY) + yOff;
    float w = _current.eyeW;
    
    // The < shape looks visually much larger than a solid rectangle of the same width.
    // If it's a playful wink, scale the < down so it visually balances with the normal eye.
    if (_current.winkRightSqueeze) {
        w = 8; // Fixed optimal size for the < shape
    }
    
    int16_t ix = (int16_t)cx;
    int16_t iy = (int16_t)cy;
    int16_t hw = (int16_t)w;
    
    // Left eye: > (points right)
    // Right eye: < (points left)
    int16_t pointX = isLeft ? ix + hw : ix - hw;
    int16_t openX  = isLeft ? ix - hw : ix + hw;
    
    float squash = 1.0f - _blinkAmount * 0.95f;
    int16_t hOffset = (int16_t)((hw - 2) * squash);
    if (hOffset < 0) hOffset = 0;
    
    int16_t topY = iy - hOffset;
    int16_t botY = iy + hOffset;
    
    // Draw thick lines
    d.drawLine(openX, topY, pointX, iy, DISPLAY_WHITE);
    d.drawLine(openX, botY, pointX, iy, DISPLAY_WHITE);
    
    // Thicken
    d.drawLine(openX, topY - 1, pointX, iy - 1, DISPLAY_WHITE);
    d.drawLine(openX, botY + 1, pointX, iy + 1, DISPLAY_WHITE);
    d.drawLine(openX, topY + 1, pointX, iy + 1, DISPLAY_WHITE);
    d.drawLine(openX, botY - 1, pointX, iy - 1, DISPLAY_WHITE);
}

void EmotionEngine::drawHappyMarks(DisplayType& d, int16_t yOff) {
    if (!_current.happyMarks) return;
    
    for (int i = 0; i < 2; i++) {
        bool isLeft = (i == 0);
        float cx = isLeft ? _current.eyeLX : _current.eyeRX;
        float cy = (isLeft ? _current.eyeLY : _current.eyeRY) + yOff;
        int16_t hw = (int16_t)_current.eyeW;
        
        int16_t ix = (int16_t)cx;
        int16_t iy = (int16_t)cy;
        
        // Horizontal pill mark below eye
        int16_t markWidth = 10;
        int16_t markX = isLeft ? ix - markWidth/2 + 2 : ix - markWidth/2 - 2;
        int16_t markY = iy + hw + 8;
        d.fillRoundRect(markX, markY, markWidth, 4, 2, DISPLAY_WHITE);
    }
}

void EmotionEngine::drawHeart(DisplayType& d, int16_t cx, int16_t cy, int16_t size) {
    int16_t r = size / 3;
    if (r < 2) r = 2;

    float squash = 1.0f - _blinkAmount * 0.95f;

    // Draw the top bumps (circles) exactly up to 1 row above their center
    // This prevents the integer math from causing a 1-pixel outcropping at the widest point
    for (int16_t dy = -r; dy <= -1; dy++) {
        int16_t circle_hw;
        if (r == 6) {
            // Hand-tuned pixel-art curve for beautifully rounded bumps (r=6)
            const int16_t hw6[] = {1, 3, 4, 5, 5, 5};
            circle_hw = hw6[dy + 6];
        } else if (r == 5) {
            // Hand-tuned pixel-art curve for r=5
            const int16_t hw5[] = {1, 3, 4, 4, 4};
            circle_hw = hw5[dy + 5];
        } else {
            // Fallback for any other sizes, with rounding
            circle_hw = (int16_t)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
        }
        
        // Left bump
        int16_t left1 = cx - r + 1 - circle_hw;
        int16_t right1 = cx - r + 1 + circle_hw;
        
        // Right bump
        int16_t left2 = cx + r - 1 - circle_hw;
        int16_t right2 = cx + r - 1 + circle_hw;
        
        int16_t squishedY = cy + (int16_t)((-r / 2 + dy) * squash);
        d.drawFastHLine(left1, squishedY, right1 - left1 + 1, DISPLAY_WHITE);
        d.drawFastHLine(left2, squishedY, right2 - left2 + 1, DISPLAY_WHITE);
    }

    // Draw the bottom 45-degree triangle starting exactly at the circle center row
    int16_t hw = 2 * r - 3; // Matches the curve of dy=-1 perfectly
    int16_t dy_bottom = 0;
    while (hw >= 0) {
        int16_t squishedY = cy + (int16_t)((-r / 2 + dy_bottom) * squash);
        d.drawFastHLine(cx - hw, squishedY, hw * 2 + 1, DISPLAY_WHITE);
        hw--;
        dy_bottom++;
    }
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

    float innerY = browCY + angle;
    float outerY = browCY - angle;

    // Mask out the eye behind the brow using a black shape
    // This allows the eyebrow to "cut" into the tall eye shape perfectly for an angry slant
    for (int i = 1; i <= 20; i++) {
        d.drawLine((int16_t)innerX, (int16_t)(innerY - i),
                   (int16_t)outerX, (int16_t)(outerY - i), DISPLAY_BLACK);
    }

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
        if (_current.mouthCurve > 2) {
            // D-shaped solid open smile (big happy grin)
            int16_t openH = (int16_t)_current.mouthOpenH;
            for (int16_t dy = 0; dy <= openH; dy++) {
                // Ellipse equation for the bottom curve
                float t = (float)dy / (float)openH;
                int16_t dx = (int16_t)(halfW * sqrtf(1.0f - t * t) + 0.5f);
                
                // Slightly round the top two corners
                if (dy == 0) dx -= 2;
                else if (dy == 1) dx -= 1;
                
                if (dx > 0) {
                    d.drawFastHLine(cx - dx, cy - openH/2 + dy, dx * 2 + 1, DISPLAY_WHITE);
                }
            }
        } else {
            // Open mouth — draw as filled white oval with black interior (O-shape)
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
        }
    } else {
        // Closed mouth — curved arc using segments
        // IMPORTANT: positive mouthCurve = smile = curve goes DOWN (higher Y on screen)
        // This is correct because on screen, Y increases downward, so a ∪ shape = smile
        float curve = _current.mouthCurve;
        int16_t leftX = cx - halfW;
        int16_t rightX = cx + halfW;

        // Draw the smile/frown as a smooth thick stroke with rounded ends
        // by overlapping filled circles along a sine curve.
        int16_t thickness = 1; // Radius of 1 (diameter 3) for a moderately thick line

        int16_t segments = (int16_t)(halfW * 2); // 1 segment per X-pixel for smooth overlap
        if (segments < 8) segments = 8;

        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            int16_t x = leftX + (int16_t)((rightX - leftX) * t);
            // Sin curve: peaks at middle, amount = curve value
            // Positive curve → positive Y offset → lower on screen → ∪ = smile
            float offset = curve * sinf(t * 3.14159f);
            int16_t y = cy + (int16_t)offset;

            // Draw filled circle to create a smooth, rounded stroke
            d.fillCircle(x, y, thickness, DISPLAY_WHITE);
        }
    }
}

void EmotionEngine::drawBlush(DisplayType& d, int16_t yOff) {
    if (_current.blushR < 1) return;

    int16_t r = (int16_t)_current.blushR;

    // Position blush below and to the outside of each eye
    int16_t ly = (int16_t)_current.eyeLY + yOff + (int16_t)_current.eyeH + 6;
    int16_t ry = (int16_t)_current.eyeRY + yOff + (int16_t)_current.eyeH + 6;
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
