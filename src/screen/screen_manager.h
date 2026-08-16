#pragma once

#include <Arduino.h>
#include "display_config.h"
#include "emotion/emotion_engine.h"
#include "clock/clock_engine.h"

// =============================================================
// ScreenManager — Engine orchestrator
// =============================================================
// Manages which engine (Emotion or Clock) is active, handles
// auto-switching, transitions, and delegates update/render.

enum class ActiveEngine : uint8_t {
    EMOTION = 0,
    CLOCK = 1
};

class ScreenManager {
public:
    void begin(EmotionEngine* emotion, ClockEngine* clock);
    void update(uint32_t deltaMs);
    void render(DisplayType& display);

    // Engine switching
    void switchEngine();                       // Toggle
    void setEngine(ActiveEngine engine);       // Direct set
    ActiveEngine getActiveEngine() const;

    // Auto-switch configuration
    void setAutoSwitch(bool enabled);
    void setAutoSwitchInterval(uint16_t seconds);
    void setClockDuration(uint16_t seconds);
    void resetAutoSwitchTimer();               // Reset timer on user interaction

    // Get frame interval from active engine
    uint32_t getFrameInterval() const;
    
    // Get remaining time before next auto-switch (in milliseconds)
    uint32_t getRemainingTime() const;

    // Access to engines
    EmotionEngine* emotionEngine();
    ClockEngine*   clockEngine();

private:
    EmotionEngine* _emotion = nullptr;
    ClockEngine*   _clock = nullptr;
    ActiveEngine   _active = ActiveEngine::EMOTION;

    // Auto-switch state
    bool     _autoSwitch = true;
    uint32_t _switchIntervalMs = 30000;
    uint32_t _clockDurationMs = 10000;
    uint32_t _autoSwitchTimer = 0;

    // Transition animation
    bool     _inTransition = false;
    int16_t  _transitionOffset = 0;
    int16_t  _transitionDir = 0;    // -1 = slide left, +1 = slide right
    uint32_t _transitionTimer = 0;
};
