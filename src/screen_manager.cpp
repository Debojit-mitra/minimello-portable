#include "screen_manager.h"
#include "config.h"

// =============================================================
// ScreenManager — Implementation
// =============================================================

void ScreenManager::begin(EmotionEngine* emotion, ClockEngine* clock) {
    _emotion = emotion;
    _clock = clock;
    _active = ActiveEngine::EMOTION;
    _autoSwitchTimer = 0;
    _inTransition = false;
}

void ScreenManager::update(uint32_t deltaMs) {
    // --- Handle transition animation ---
    if (_inTransition) {
        _transitionTimer += deltaMs;

        // Calculate progress (0.0 → 1.0)
        float progress = (float)_transitionTimer / TRANSITION_DURATION_MS;
        if (progress >= 1.0f) {
            progress = 1.0f;
            _inTransition = false;
            _transitionOffset = 0;
        } else {
            // Ease-out cubic
            float t = 1.0f - progress;
            t = 1.0f - t * t * t;
            _transitionOffset = (int16_t)(SCREEN_WIDTH * (1.0f - t) * _transitionDir);
        }
    }

    // --- Update active engine ---
    if (_active == ActiveEngine::EMOTION) {
        _emotion->update(deltaMs);
    } else {
        _clock->update(deltaMs);
    }

    // --- Auto-switch logic ---
    if (_autoSwitch && !_inTransition) {
        _autoSwitchTimer += deltaMs;

        if (_active == ActiveEngine::EMOTION) {
            // In emotion mode: switch to clock after interval
            if (_autoSwitchTimer >= _switchIntervalMs) {
                _autoSwitchTimer = 0;
                _autoSwitchedToClock = true;
                setEngine(ActiveEngine::CLOCK);
            }
        } else if (_autoSwitchedToClock) {
            // Auto-switched to clock: return to emotion after duration
            if (_autoSwitchTimer >= _clockDurationMs) {
                _autoSwitchTimer = 0;
                _autoSwitchedToClock = false;
                setEngine(ActiveEngine::EMOTION);
            }
        }
    }
}

void ScreenManager::render(DisplayType& display) {
    if (_inTransition) {
        // During transition, we can't easily render both screens
        // since Adafruit GFX doesn't support viewport clipping.
        // Instead, just render the new screen with a slight offset fade.
        // We'll use the offset for a simple visual effect.
        if (_active == ActiveEngine::EMOTION) {
            _emotion->render(display);
        } else {
            _clock->render(display);
        }
    } else {
        if (_active == ActiveEngine::EMOTION) {
            _emotion->render(display);
        } else {
            _clock->render(display);
        }
    }
}

void ScreenManager::switchEngine() {
    if (_active == ActiveEngine::EMOTION) {
        setEngine(ActiveEngine::CLOCK);
    } else {
        setEngine(ActiveEngine::EMOTION);
    }
    // Manual switch resets auto-switch timer
    _autoSwitchTimer = 0;
    _autoSwitchedToClock = false;
}

void ScreenManager::setEngine(ActiveEngine engine) {
    if (engine == _active && !_inTransition) return;

    _active = engine;
    _inTransition = true;
    _transitionTimer = 0;
    _transitionDir = (engine == ActiveEngine::CLOCK) ? -1 : 1;
    _autoSwitchTimer = 0;
    _autoSwitchedToClock = false;
}

ActiveEngine ScreenManager::getActiveEngine() const {
    return _active;
}

void ScreenManager::setAutoSwitch(bool enabled) {
    _autoSwitch = enabled;
    _autoSwitchTimer = 0;
}

void ScreenManager::resetAutoSwitchTimer() {
    _autoSwitchTimer = 0;
    _autoSwitchedToClock = false;
}

void ScreenManager::setAutoSwitchInterval(uint16_t seconds) {
    _switchIntervalMs = (uint32_t)seconds * 1000;
}

void ScreenManager::setClockDuration(uint16_t seconds) {
    _clockDurationMs = (uint32_t)seconds * 1000;
}

uint32_t ScreenManager::getFrameInterval() const {
    if (_inTransition) return 1000 / EMOTION_FPS_ACTIVE;  // Fast during transition
    if (_active == ActiveEngine::EMOTION) {
        return _emotion->getFrameInterval();
    }
    return _clock->getFrameInterval();
}

EmotionEngine* ScreenManager::emotionEngine() {
    return _emotion;
}

ClockEngine* ScreenManager::clockEngine() {
    return _clock;
}
