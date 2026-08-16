#include "emotion_engine.h"
#include "config.h"
#include "clock/clock_engine.h"
#include "bitmaps/icons.h"
#include <math.h>

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

