#include "emotion_engine.h"
#include "config.h"
#include "clock/clock_engine.h"
#include "bitmaps/icons.h"
#include <math.h>

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

