#pragma once

#include <Arduino.h>

// =============================================================
// TouchManager — TTP223 capacitive touch with debounce
// =============================================================
// Detects: TAP, LONG_PRESS, DOUBLE_TAP
// TTP223 outputs HIGH when touched on GPIO2.
// Uses ISR for responsive input, processed in main loop.

enum class TouchEvent : uint8_t {
    NONE = 0,
    TAP,
    LONG_PRESS,
    DOUBLE_TAP
};

// Callback function type
using TouchCallback = void (*)(TouchEvent);

class TouchManager {
public:
    void begin(uint8_t pin);
    void update();    // Call every loop iteration

    void onEvent(TouchCallback cb);

    bool isTouched() const;
    bool isLongPressing() const;

private:
    uint8_t         _pin = 0;
    TouchCallback   _callback = nullptr;

    // ISR communication
    static volatile bool _isrFlag;
    static void IRAM_ATTR _isrHandler();

    // State machine
    bool     _lastState = false;
    bool     _currentState = false;
    bool     _longPressTriggered = false;

    uint32_t _pressStartMs = 0;
    uint32_t _lastReleaseMs = 0;
    uint32_t _debounceMs = 0;

    uint8_t  _tapCount = 0;
    bool     _waitingForDoubleTap = false;

    void emit(TouchEvent event);
};
