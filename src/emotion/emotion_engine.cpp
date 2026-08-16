#include "emotion_engine.h"

#include "config.h"
#include "clock/clock_engine.h"  // for WeatherCondition
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

