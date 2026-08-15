#pragma once

#include <Arduino.h>
#include "display_config.h"

// =============================================================
// EmotionEngine — Procedural animated face system
// =============================================================
// All faces are rendered procedurally using GFX primitives.
// No full-frame bitmaps — each emotion is defined by a set of
// float parameters that are smoothly interpolated for transitions.
//
// Personality system: The character has an internal mood that
// naturally drifts between emotions over time, influenced by
// touch interactions and (optionally) weather data.

enum class Emotion : uint8_t {
    NEUTRAL = 0,
    HAPPY,
    SAD,
    ANGRY,
    SURPRISED,
    SLEEPY,
    LOVE,
    WINK,
    COUNT  // sentinel for iteration
};

enum class WeatherCondition : uint8_t;  // Forward declare from clock_engine.h

// Parameters that define the visual appearance of a face.
// All coordinates are relative to the screen center.
struct FaceParams {
    // Eye geometry (ellipse half-widths)
    float eyeW;            // Eye width (half)
    float eyeH;            // Eye height (half)

    // Eye positions (center of each eye)
    float eyeLX, eyeLY;    // Left eye center
    float eyeRX, eyeRY;    // Right eye center

    // Pupil
    float pupilR;           // Pupil radius
    float pupilOX, pupilOY; // Pupil offset from eye center

    // Eyebrows
    float browOffY;         // Vertical offset above eye top
    float browLAngle;       // Left brow angle (+ = inner up / angry)
    float browRAngle;       // Right brow angle
    float browLen;          // Brow length
    bool  browVisible;      // Whether to draw eyebrows

    // Mouth
    float mouthY;           // Mouth center Y
    float mouthW;           // Mouth width (half)
    float mouthCurve;       // Positive = smile, negative = frown
    float mouthOpenH;       // Open mouth height (0 = closed line)

    // Effects
    float blushR;           // Blush circle radius (0 = hidden)
    float bounce;           // Vertical bounce offset

    // Special eye modes (mutually exclusive with normal eyes)
    bool  heartEyes;        // Draw hearts instead of eyes
    bool  arcEyes;          // Draw ^_^ arc eyes (happy squint)
    bool  winkLeft;         // Left eye closed as arc (wink)
};

// Floating particle (hearts, stars, Zzz)
struct Particle {
    float x, y;
    float vx, vy;
    float life;     // Remaining life in seconds
    uint8_t type;   // 0=heart, 1=star, 2=zzz
    bool active;
};

#define MAX_PARTICLES 5

// --- Mood / Personality Configuration ---
#define DEFAULT_MOOD_INTERVAL_S    300  // Default: change mood every 5 minutes
#define MIN_MOOD_INTERVAL_S        60   // Minimum: 1 minute
#define MAX_MOOD_INTERVAL_S        900  // Maximum: 15 minutes
#define MOOD_HOLD_MIN_S            20   // Hold a mood for at least 20 seconds
#define MOOD_HOLD_MAX_S            90   // Hold a mood for at most 90 seconds
#define INTERACTION_HAPPY_THRESHOLD 5   // Touches within window to boost happiness
#define INTERACTION_WINDOW_MS      60000 // 1 minute window for touch counting

class EmotionEngine {
public:
    void begin();
    void update(uint32_t deltaMs);
    void render(DisplayType& display);

    void setEmotion(Emotion e);
    Emotion getEmotion() const;
    const char* getEmotionName() const;

    // Touch reaction: context-aware response
    void onTouch();

    // Time-based mood: called with current time and night mode status
    void checkTimeBasedMood(uint8_t hour, bool isNight);

    // Weather influence: subtle mood bias based on weather
    void setWeatherCondition(WeatherCondition condition);

    // Mood frequency (configurable from WebUI)
    void setMoodInterval(uint16_t seconds);

    // Returns appropriate frame interval in ms
    uint32_t getFrameInterval() const;

private:
    Emotion     _emotion = Emotion::NEUTRAL;
    Emotion     _prevEmotion = Emotion::NEUTRAL;
    FaceParams  _current;       // Currently displayed state
    FaceParams  _target;        // Target state we're interpolating toward
    bool        _transitioning = false;

    // Blink system
    int32_t     _blinkTimer = 0;
    bool        _isBlinking = false;
    uint32_t    _blinkPhase = 0;
    float       _blinkAmount = 0;   // 0 = open, 1 = closed

    // Idle micro-movements
    uint32_t    _idleTime = 0;
    float       _idlePupilX = 0;
    float       _idlePupilY = 0;
    float       _idleBounce = 0;

    // Touch reaction
    bool        _touchReacting = false;
    uint32_t    _touchReactionTimer = 0;

    // --- Personality / Mood System ---
    uint32_t    _moodTimer = 0;         // Timer for ambient mood changes
    uint32_t    _moodIntervalMs = DEFAULT_MOOD_INTERVAL_S * 1000;
    uint32_t    _moodHoldTimer = 0;     // How long current mood has been held
    uint32_t    _moodHoldDuration = 0;  // How long to hold before returning to neutral
    bool        _inAmbientMood = false; // Currently in an ambient (non-neutral) mood
    bool        _moodOverridden = false; // Mood was set externally (WebUI/touch/night)

    // Touch interaction tracking
    uint8_t     _recentTouches = 0;     // Touch count in recent window
    uint32_t    _touchWindowStart = 0;  // Start of current touch counting window

    // Weather bias
    WeatherCondition _weatherCondition;
    bool        _hasWeather = false;

    // Particles
    Particle    _particles[MAX_PARTICLES];

    // Get target params for a given emotion
    static FaceParams getEmotionParams(Emotion e);

    // Interpolate current toward target
    void lerpParams(float dt);

    // Animation sub-systems
    void updateBlink(uint32_t deltaMs);
    void updateIdle(uint32_t deltaMs);
    void updateParticles(uint32_t deltaMs);
    void updateTouchReaction(uint32_t deltaMs);
    void updateMood(uint32_t deltaMs);

    // Mood selection
    Emotion pickAmbientMood();

    // Rendering sub-functions
    void drawEye(DisplayType& d, bool isLeft, int16_t yOff);
    void drawArcEye(DisplayType& d, bool isLeft, int16_t yOff);
    void drawHeart(DisplayType& d, int16_t cx, int16_t cy, int16_t size);
    void drawEyebrow(DisplayType& d, bool isLeft, int16_t yOff);
    void drawMouth(DisplayType& d, int16_t yOff);
    void drawBlush(DisplayType& d, int16_t yOff);
    void drawParticles(DisplayType& d);

    // Particle spawning
    void spawnParticle(uint8_t type);
};
