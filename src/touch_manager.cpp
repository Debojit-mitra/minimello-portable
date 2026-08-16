#include "touch_manager.h"
#include "config.h"

// =============================================================
// TouchManager — Implementation
// =============================================================

volatile bool TouchManager::_isrFlag = false;

void IRAM_ATTR TouchManager::_isrHandler() {
    _isrFlag = true;
}

void TouchManager::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(_pin), _isrHandler, CHANGE);
}

void TouchManager::update() {
    uint32_t now = millis();

    // Read current pin state (with ISR flag for responsiveness)
    bool rawState = digitalRead(_pin);

    // Debounce
    if (rawState != _currentState) {
        if (now - _debounceMs < TOUCH_DEBOUNCE_MS) {
            return;  // Still bouncing
        }
        _debounceMs = now;
        _currentState = rawState;
    }

    // Clear ISR flag
    _isrFlag = false;

    // --- State transitions ---

    // PRESS detected (rising edge)
    if (_currentState && !_lastState) {
        _pressStartMs = now;
        _longPressTriggered = false;
        _veryLongPressTriggered = false;
    }

    // HELD — check for long press
    if (_currentState && _lastState) {
        uint32_t holdTime = now - _pressStartMs;
        
        if (!_longPressTriggered && holdTime >= TOUCH_LONG_PRESS_MS) {
            _longPressTriggered = true;
            emit(TouchEvent::LONG_PRESS);
        }
        
        if (!_veryLongPressTriggered && holdTime >= TOUCH_VERY_LONG_PRESS_MS) {
            _veryLongPressTriggered = true;
            emit(TouchEvent::VERY_LONG_PRESS);
        }
    }

    // RELEASE detected (falling edge)
    if (!_currentState && _lastState) {
        uint32_t pressDuration = now - _pressStartMs;

        // Only count as tap if it wasn't a long press
        if (!_longPressTriggered && pressDuration < TOUCH_LONG_PRESS_MS) {
            _tapCount++;

            if (_tapCount == 1) {
                _waitingForDoubleTap = true;
                _lastReleaseMs = now;
            }
        }
    }

    // Check for double-tap timeout
    if (_waitingForDoubleTap && !_currentState) {
        if (_tapCount >= 2) {
            // Double tap detected
            emit(TouchEvent::DOUBLE_TAP);
            _tapCount = 0;
            _waitingForDoubleTap = false;
        } else if (now - _lastReleaseMs >= TOUCH_DOUBLE_TAP_MS) {
            // Timeout — single tap
            emit(TouchEvent::TAP);
            _tapCount = 0;
            _waitingForDoubleTap = false;
        }
    }

    _lastState = _currentState;
}

void TouchManager::onEvent(TouchCallback cb) {
    _callback = cb;
}

bool TouchManager::isTouched() const {
    return _currentState;
}

bool TouchManager::isLongPressing() const {
    return _currentState && _longPressTriggered;
}

void TouchManager::emit(TouchEvent event) {
    if (_callback) {
        _callback(event);
    }
}
