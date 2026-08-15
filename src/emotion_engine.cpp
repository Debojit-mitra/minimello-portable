#include "emotion_engine.h"
#include "config.h"
#include "clock_engine.h"  // for WeatherCondition
#include "bitmaps/icons.h"
#include <math.h>

// =============================================================
// EmotionEngine — Implementation
// =============================================================

// --- Emotion Parameter Presets ---
// Each emotion is a set of FaceParams defining eye shape, position,
// mouth curve, eyebrow angle, etc. The engine smoothly interpolates
// between these states for fluid transitions.
//
// Coordinate system: screen is 128x64. Face is centered around (64, 32).
// Eye positions are absolute screen coordinates.
// Mouth: positive mouthCurve = smile (curve below center = U shape).

FaceParams EmotionEngine::getEmotionParams(Emotion e) {
    FaceParams p;
    // Common defaults
    p.eyeW = 13;  p.eyeH = 16;
    p.eyeLX = 40; p.eyeLY = 25;
    p.eyeRX = 88; p.eyeRY = 25;
    p.pupilR = 4;
    p.pupilOX = 0; p.pupilOY = 1;
    p.browOffY = 8;
    p.browLAngle = 0; p.browRAngle = 0;
    p.browLen = 14;
    p.browVisible = false;
    p.mouthY = 48;
    p.mouthW = 12;
    p.mouthCurve = 0;
    p.mouthOpenH = 0;
    p.blushR = 0;
    p.bounce = 0;
    p.heartEyes = false;
    p.arcEyes = false;
    p.winkLeft = false;

    switch (e) {
        case Emotion::NEUTRAL:
            // Calm, friendly face with a gentle smile
            p.mouthCurve = 4;    // Warm visible smile
            p.mouthW = 10;
            break;

        case Emotion::HAPPY:
            // Classic ^_^ kawaii happy — arc eyes, big smile, blush, bounce
            p.arcEyes = true;
            p.eyeLY = 27;
            p.eyeRY = 27;
            p.mouthCurve = 10;   // Big wide smile
            p.mouthW = 16;
            p.blushR = 5;
            p.bounce = -2;       // Upward bounce
            break;

        case Emotion::SAD:
            // Droopy eyes tilted inward at top, big visible frown, no brows
            p.eyeW = 12;
            p.eyeH = 14;
            p.eyeLX = 42;  p.eyeLY = 28;
            p.eyeRX = 86;  p.eyeRY = 28;
            p.pupilR = 3;
            p.pupilOY = 3;        // Looking down
            p.mouthCurve = -8;    // Clear frown
            p.mouthW = 12;
            p.mouthY = 50;
            p.bounce = 3;         // Droopy
            break;

        case Emotion::ANGRY:
            // Narrow eyes with thick overlapping flat-top brows, tight frown
            p.eyeW = 14;
            p.eyeH = 10;
            p.eyeLY = 28;
            p.eyeRY = 28;
            p.pupilR = 3;
            p.pupilOY = 0;
            p.browVisible = true;
            p.browLAngle = 8;     // Strong inward-down angle
            p.browRAngle = 8;
            p.browLen = 18;
            p.browOffY = 4;       // Closer to eyes — more menacing
            p.mouthCurve = -6;    // Tight frown
            p.mouthW = 10;
            p.mouthY = 49;
            break;

        case Emotion::SURPRISED:
            // Wide round eyes, tiny pupils, open O mouth, jump up
            p.eyeW = 16;
            p.eyeH = 20;
            p.eyeLY = 23;
            p.eyeRY = 23;
            p.pupilR = 2;         // Tiny startled pupils
            p.pupilOY = 0;
            p.mouthCurve = 0;
            p.mouthOpenH = 10;    // Open mouth (O shape)
            p.mouthW = 6;
            p.mouthY = 49;
            p.bounce = -3;        // Jump up
            break;

        case Emotion::SLEEPY:
            // Very squished eyes (almost closed), small yawn
            p.eyeH = 3;
            p.eyeLY = 28;
            p.eyeRY = 28;
            p.pupilR = 2;
            p.pupilOY = 0;
            p.mouthCurve = 1;
            p.mouthW = 8;
            p.mouthOpenH = 5;     // Small yawn
            p.mouthY = 48;
            p.bounce = 3;         // Droopy
            break;

        case Emotion::LOVE:
            // Heart-shaped eyes, big smile, blush, floating hearts
            p.heartEyes = true;
            p.eyeW = 14;
            p.eyeH = 14;
            p.eyeLY = 25;
            p.eyeRY = 25;
            p.mouthCurve = 8;
            p.mouthW = 14;
            p.blushR = 5;
            p.bounce = -1;
            break;

        case Emotion::WINK:
            // Left eye closed as arc, right eye normal with slight smile
            p.winkLeft = true;
            p.eyeLY = 27;
            p.eyeRY = 25;
            p.mouthCurve = 6;     // Cheeky smile
            p.mouthW = 12;
            p.blushR = 3;         // Subtle blush
            break;

        default:
            break;
    }
    return p;
}

// --- Lifecycle ---

void EmotionEngine::begin() {
    _emotion = Emotion::NEUTRAL;
    _target = getEmotionParams(Emotion::NEUTRAL);
    _current = _target;
    _blinkTimer = random(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);

    // Initialize mood system
    _moodTimer = 0;
    _moodHoldTimer = 0;
    _inAmbientMood = false;
    _moodOverridden = false;
    _recentTouches = 0;
    _touchWindowStart = millis();
    _hasWeather = false;

    // Initialize particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        _particles[i].active = false;
    }
}

void EmotionEngine::update(uint32_t deltaMs) {
    float dt = deltaMs / 1000.0f;

    // Interpolate face parameters toward target
    lerpParams(dt);

    // Update sub-systems
    updateBlink(deltaMs);
    updateIdle(deltaMs);
    updateTouchReaction(deltaMs);
    updateMood(deltaMs);
    updateParticles(deltaMs);

    // Spawn particles for certain emotions
    if (_emotion == Emotion::LOVE && random(100) < 3) {
        spawnParticle(0);  // hearts
    }
    if (_emotion == Emotion::SLEEPY && random(100) < 2) {
        spawnParticle(2);  // zzz
    }
    if (_emotion == Emotion::HAPPY && random(100) < 1) {
        spawnParticle(1);  // stars (rare sparkle when happy)
    }
}

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

void EmotionEngine::setEmotion(Emotion e) {
    if (e == _emotion) return;
    _prevEmotion = _emotion;
    _emotion = e;
    _target = getEmotionParams(e);
    _transitioning = true;
}

Emotion EmotionEngine::getEmotion() const {
    return _emotion;
}

const char* EmotionEngine::getEmotionName() const {
    static const char* names[] = {
        "Neutral", "Happy", "Sad", "Angry",
        "Surprised", "Sleepy", "Love", "Wink"
    };
    return names[(uint8_t)_emotion];
}

void EmotionEngine::onTouch() {
    if (_touchReacting) return;
    _touchReacting = true;
    _touchReactionTimer = 0;

    // Track touch for happiness bonus
    _recentTouches++;

    // Context-aware reaction based on current emotion
    _prevEmotion = _emotion;

    switch (_emotion) {
        case Emotion::NEUTRAL:
        case Emotion::WINK: {
            // Random fun reaction
            int r = random(100);
            if (r < 40)      setEmotion(Emotion::HAPPY);
            else if (r < 65) setEmotion(Emotion::SURPRISED);
            else if (r < 85) setEmotion(Emotion::LOVE);
            else             setEmotion(Emotion::WINK);
            break;
        }
        case Emotion::HAPPY:
            // Already happy — bounce + chance of love
            if (random(100) < 30) {
                setEmotion(Emotion::LOVE);
            } else {
                // Re-trigger happy for a bounce effect
                _target.bounce = -4;
                setEmotion(Emotion::HAPPY);
            }
            break;
        case Emotion::SAD:
            // Comforting touch → cheers up
            setEmotion(Emotion::HAPPY);
            break;
        case Emotion::ANGRY:
            // Touch calms down (usually) or gets more annoyed
            if (random(100) < 75) {
                setEmotion(Emotion::NEUTRAL);
            } else {
                setEmotion(Emotion::SURPRISED);
            }
            break;
        case Emotion::SLEEPY:
            // Startled awake briefly
            setEmotion(Emotion::SURPRISED);
            break;
        case Emotion::SURPRISED:
            // Already surprised — settle into happy
            setEmotion(Emotion::HAPPY);
            break;
        case Emotion::LOVE:
            // Extra love → wink
            setEmotion(Emotion::WINK);
            break;
        default:
            setEmotion(Emotion::SURPRISED);
            break;
    }
}

void EmotionEngine::checkTimeBasedMood(uint8_t hour, bool isNight) {
    if (_touchReacting) return;  // Don't override touch reactions
    if (_moodOverridden) return; // Don't override WebUI-set emotions

    if (isNight) {
        if (_emotion != Emotion::SLEEPY) setEmotion(Emotion::SLEEPY);
    } else if (hour >= 6 && hour < 9) {
        if (_emotion == Emotion::SLEEPY) setEmotion(Emotion::HAPPY);
    } else {
        // During the day, clear sleepy state
        if (_emotion == Emotion::SLEEPY) {
            setEmotion(Emotion::NEUTRAL);
        }
    }
}

void EmotionEngine::setWeatherCondition(WeatherCondition condition) {
    _weatherCondition = condition;
    _hasWeather = true;
}

void EmotionEngine::setMoodInterval(uint16_t seconds) {
    if (seconds < MIN_MOOD_INTERVAL_S) seconds = MIN_MOOD_INTERVAL_S;
    if (seconds > MAX_MOOD_INTERVAL_S) seconds = MAX_MOOD_INTERVAL_S;
    _moodIntervalMs = (uint32_t)seconds * 1000;
}

uint32_t EmotionEngine::getFrameInterval() const {
    if (_transitioning || _isBlinking || _touchReacting) {
        return 1000 / EMOTION_FPS_ACTIVE;
    }
    return 1000 / EMOTION_FPS_IDLE;
}

// --- Interpolation ---

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

void EmotionEngine::lerpParams(float dt) {
    // Exponential ease-out interpolation
    float t = 1.0f - powf(0.05f, dt * EMOTION_LERP_SPEED);

    _current.eyeW     = lerpf(_current.eyeW,     _target.eyeW,     t);
    _current.eyeH     = lerpf(_current.eyeH,     _target.eyeH,     t);
    _current.eyeLX    = lerpf(_current.eyeLX,    _target.eyeLX,    t);
    _current.eyeLY    = lerpf(_current.eyeLY,    _target.eyeLY,    t);
    _current.eyeRX    = lerpf(_current.eyeRX,    _target.eyeRX,    t);
    _current.eyeRY    = lerpf(_current.eyeRY,    _target.eyeRY,    t);
    _current.pupilR   = lerpf(_current.pupilR,   _target.pupilR,   t);
    _current.pupilOX  = lerpf(_current.pupilOX,  _target.pupilOX,  t);
    _current.pupilOY  = lerpf(_current.pupilOY,  _target.pupilOY,  t);
    _current.browOffY = lerpf(_current.browOffY, _target.browOffY, t);
    _current.browLAngle = lerpf(_current.browLAngle, _target.browLAngle, t);
    _current.browRAngle = lerpf(_current.browRAngle, _target.browRAngle, t);
    _current.browLen  = lerpf(_current.browLen,  _target.browLen,  t);
    _current.mouthY   = lerpf(_current.mouthY,   _target.mouthY,   t);
    _current.mouthW   = lerpf(_current.mouthW,   _target.mouthW,   t);
    _current.mouthCurve = lerpf(_current.mouthCurve, _target.mouthCurve, t);
    _current.mouthOpenH = lerpf(_current.mouthOpenH, _target.mouthOpenH, t);
    _current.blushR   = lerpf(_current.blushR,   _target.blushR,   t);
    _current.bounce   = lerpf(_current.bounce,   _target.bounce,   t);

    // Snap booleans when close enough
    _current.browVisible = _target.browVisible;
    _current.heartEyes = _target.heartEyes;
    _current.arcEyes = _target.arcEyes;
    _current.winkLeft = _target.winkLeft;

    // Check if transition is complete (all values close to target)
    float maxDiff = fabsf(_current.eyeH - _target.eyeH) +
                    fabsf(_current.mouthCurve - _target.mouthCurve) +
                    fabsf(_current.bounce - _target.bounce);
    if (maxDiff < 0.5f) {
        _transitioning = false;
    }
}

// --- Animation Sub-Systems ---

void EmotionEngine::updateBlink(uint32_t deltaMs) {
    if (_emotion == Emotion::SLEEPY) return;  // Already has closed eyes

    _blinkTimer -= (int32_t)deltaMs;
    if (_blinkTimer <= 0 && !_isBlinking) {
        _blinkTimer = random(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
        _isBlinking = true;
        _blinkPhase = 0;
    }

    if (_isBlinking) {
        _blinkPhase += deltaMs;
        uint32_t half = BLINK_DURATION_MS / 2;
        if (_blinkPhase < half) {
            // Closing
            _blinkAmount = (float)_blinkPhase / (float)half;
        } else if (_blinkPhase < BLINK_DURATION_MS) {
            // Opening
            _blinkAmount = 1.0f - (float)(_blinkPhase - half) / (float)half;
        } else {
            _isBlinking = false;
            _blinkAmount = 0;
        }
    }
}

void EmotionEngine::updateIdle(uint32_t deltaMs) {
    _idleTime += deltaMs;
    float t = _idleTime / 1000.0f;

    // Subtle pupil drift using sine waves
    _idlePupilX = sinf(t * 0.7f) * 1.5f;
    _idlePupilY = cosf(t * 0.5f) * 1.0f;

    // Very subtle body breathing/bounce
    _idleBounce = sinf(t * 1.5f) * 0.8f;
}

void EmotionEngine::updateTouchReaction(uint32_t deltaMs) {
    if (!_touchReacting) return;

    _touchReactionTimer += deltaMs;

    // End reaction after duration — return to previous or neutral
    if (_touchReactionTimer >= TOUCH_REACTION_DURATION) {
        _touchReacting = false;
        _touchReactionTimer = 0;
        _moodOverridden = false;

        // If we were in an ambient mood, let the mood system take over
        // Otherwise return to neutral
        if (!_inAmbientMood) {
            setEmotion(Emotion::NEUTRAL);
        }
    }
}

// --- Mood / Personality System ---

void EmotionEngine::updateMood(uint32_t deltaMs) {
    // Don't cycle moods during touch reactions, night mode, or external overrides
    if (_touchReacting) return;
    if (_emotion == Emotion::SLEEPY) return;

    // Track touch interaction window
    uint32_t now = millis();
    if (now - _touchWindowStart >= INTERACTION_WINDOW_MS) {
        _touchWindowStart = now;
        _recentTouches = 0;
    }

    // If currently in an ambient mood, count down hold timer
    if (_inAmbientMood) {
        _moodHoldTimer += deltaMs;
        if (_moodHoldTimer >= _moodHoldDuration) {
            // Mood duration expired — return to neutral
            _inAmbientMood = false;
            _moodHoldTimer = 0;
            setEmotion(Emotion::NEUTRAL);
        }
        return;
    }

    // If moodOverridden (set by WebUI), don't auto-change
    if (_moodOverridden) return;

    // Count down to next mood change
    _moodTimer += deltaMs;
    if (_moodTimer >= _moodIntervalMs) {
        _moodTimer = 0;

        // Pick a random ambient mood
        Emotion newMood = pickAmbientMood();
        if (newMood != Emotion::NEUTRAL) {
            _inAmbientMood = true;
            _moodHoldTimer = 0;
            _moodHoldDuration = random(MOOD_HOLD_MIN_S, MOOD_HOLD_MAX_S) * 1000;
            setEmotion(newMood);
        }
    }
}

Emotion EmotionEngine::pickAmbientMood() {
    // Base weights for each mood (out of 100)
    // The character naturally has a personality — it's not purely random
    int wNeutral   = 30;
    int wHappy     = 25;
    int wSad       = 8;
    int wAngry     = 4;
    int wSurprised = 8;
    int wLove      = 12;
    int wWink      = 8;
    // Sleepy is handled by time-based system, not ambient

    // --- Interaction bonus: lots of recent touches → happier character ---
    if (_recentTouches >= INTERACTION_HAPPY_THRESHOLD) {
        wHappy     += 15;
        wLove      += 10;
        wSad       -= 5;
        wAngry     -= 3;
    }

    // --- Weather bias (subtle, not dominant) ---
    if (_hasWeather) {
        switch (_weatherCondition) {
            case WeatherCondition::CLEAR:
                wHappy += 8;
                wLove  += 3;
                break;
            case WeatherCondition::RAIN:
                wSad   += 5;
                wHappy -= 3;
                break;
            case WeatherCondition::THUNDER:
                wSurprised += 6;
                wAngry += 3;
                break;
            case WeatherCondition::CLOUDS:
                wNeutral += 5;
                break;
            case WeatherCondition::MIST:
                wSad += 3;
                wNeutral += 3;
                break;
            case WeatherCondition::SNOW:
                wSurprised += 5;
                wHappy += 3;
                break;
            default:
                break;
        }
    }

    // Clamp all weights to >= 0
    if (wNeutral < 0) wNeutral = 0;
    if (wHappy < 0) wHappy = 0;
    if (wSad < 0) wSad = 0;
    if (wAngry < 0) wAngry = 0;
    if (wSurprised < 0) wSurprised = 0;
    if (wLove < 0) wLove = 0;
    if (wWink < 0) wWink = 0;

    int total = wNeutral + wHappy + wSad + wAngry + wSurprised + wLove + wWink;
    if (total <= 0) return Emotion::NEUTRAL;

    int roll = random(total);

    if (roll < wNeutral) return Emotion::NEUTRAL;
    roll -= wNeutral;
    if (roll < wHappy) return Emotion::HAPPY;
    roll -= wHappy;
    if (roll < wSad) return Emotion::SAD;
    roll -= wSad;
    if (roll < wAngry) return Emotion::ANGRY;
    roll -= wAngry;
    if (roll < wSurprised) return Emotion::SURPRISED;
    roll -= wSurprised;
    if (roll < wLove) return Emotion::LOVE;
    roll -= wLove;
    if (roll < wWink) return Emotion::WINK;

    return Emotion::NEUTRAL;
}

void EmotionEngine::updateParticles(uint32_t deltaMs) {
    float dt = deltaMs / 1000.0f;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!_particles[i].active) continue;

        _particles[i].x += _particles[i].vx * dt;
        _particles[i].y += _particles[i].vy * dt;
        _particles[i].life -= dt;

        // Deactivate if expired or off screen
        if (_particles[i].life <= 0 ||
            _particles[i].y < -10 || _particles[i].y > 70 ||
            _particles[i].x < -10 || _particles[i].x > 138) {
            _particles[i].active = false;
        }
    }
}

void EmotionEngine::spawnParticle(uint8_t type) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (_particles[i].active) continue;

        _particles[i].active = true;
        _particles[i].type = type;
        _particles[i].life = 2.0f + random(100) / 100.0f;

        if (type == 0) {
            // Hearts: float up from sides
            _particles[i].x = random(20, 108);
            _particles[i].y = 50 + random(10);
            _particles[i].vx = random(-10, 10);
            _particles[i].vy = -15 - random(10);
        } else if (type == 1) {
            // Stars: sparkle around the head area
            _particles[i].x = random(25, 103);
            _particles[i].y = 10 + random(20);
            _particles[i].vx = random(-8, 8);
            _particles[i].vy = -10 - random(5);
        } else if (type == 2) {
            // Zzz: drift up-right from head area
            _particles[i].x = 85 + random(10);
            _particles[i].y = 15 + random(5);
            _particles[i].vx = 5 + random(5);
            _particles[i].vy = -8 - random(5);
        }
        break;
    }
}

// =============================================================
// Rendering
// =============================================================

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
