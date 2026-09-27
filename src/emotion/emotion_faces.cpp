#include "emotion_engine.h"
#include "config.h"
#include "clock/clock_engine.h"
#include "bitmaps/icons.h"
#include <math.h>

FaceParams EmotionEngine::getEmotionParams(Emotion e) {
    FaceParams p;
    // Common defaults
    p.eyeW = 11;  p.eyeH = 13;
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
    p.squeezeEyes = false;
    p.winkRightSqueeze = false;
    p.happyMarks = false;

    switch (e) {
        case Emotion::NEUTRAL:
            // Calm, friendly face with a gentle smile
            p.mouthCurve = 4;    // Warm visible smile
            p.mouthW = 10;
            break;

        case Emotion::HAPPY:
            // Classic ^_^ kawaii happy — arc eyes, normal smile, blush, bounce
            p.arcEyes = true;
            p.eyeLY = 27;
            p.eyeRY = 27;
            p.mouthCurve = 5;    // Normal wide smile
            p.mouthOpenH = 0;
            p.mouthW = 9;
            p.blushR = 5;
            p.bounce = -2;       // Upward bounce
            break;

        case Emotion::SAD:
            // Droopy eyes tilted inward at top, big visible frown, no brows
            p.eyeW = 10;
            p.eyeH = 12;
            p.eyeLX = 42;  p.eyeLY = 28;
            p.eyeRX = 86;  p.eyeRY = 28;
            p.pupilR = 3;
            p.pupilOY = 3;        // Looking down
            p.mouthCurve = -4;    // Subtle frown
            p.mouthW = 9;         // Shorter mouth
            p.mouthY = 50;
            p.bounce = 3;         // Droopy
            break;

        case Emotion::ANGRY:
            // Standard eyes with sharp angled brows, tight frown
            p.eyeLY = 28;         // Bring eyeballs down
            p.eyeRY = 28;
            p.browVisible = true;
            p.browLAngle = 8;     // Strong inward-down angle
            p.browRAngle = 8;
            p.browLen = 36;       // Wide enough to completely mask the top of the eye without leaving spikes
            p.browOffY = -9;      // Pulls brows down even deeper for a heavier cut
            p.mouthCurve = -6;    // Tight frown
            p.mouthW = 8;
            p.mouthY = 49;
            break;

        case Emotion::SURPRISED:
            // Standard eyes, no brows, tiny pupils, open O mouth
            p.pupilR = 2;         // Tiny startled pupils
            p.pupilOY = 0;
            p.browVisible = false;
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
            p.eyeW = 11;
            p.eyeH = 11;
            p.eyeLY = 25;
            p.eyeRY = 25;
            p.mouthCurve = 6;
            p.mouthW = 12;
            p.blushR = 5;
            p.bounce = -1;
            break;

        case Emotion::WINK:
            // Left eye closed as arc, right eye normal with slight smile
            p.winkLeft = true;
            p.eyeLY = 27;
            p.eyeRY = 25;
            p.eyeH = 14;
            // p.pupilR = 4;         // Smaller pupil
            p.mouthCurve = 6;     // Cheeky smile
            p.mouthW = 12;
            p.blushR = 3;         // Subtle blush
            break;

        case Emotion::JOYFUL:
            // ^_^ eyes with D-shaped smile and blush
            p.arcEyes = true;
            p.eyeW = 10;          // Wider arcs
            p.eyeLY = 27;
            p.eyeRY = 27;
            p.mouthCurve = 5;     // D-shape trigger
            p.mouthOpenH = 10;
            p.mouthW = 12;
            p.blushR = 5;         // Use standard blush instead of custom marks
            p.bounce = -2;
            break;

        case Emotion::EXCITED:
            // > < tight closed eyes with D-shaped smile and blush
            p.squeezeEyes = true;
            p.eyeW = 8;           // Smaller size for the > <
            p.eyeLY = 26;         // Shift up slightly
            p.eyeRY = 26;
            p.mouthCurve = 5;     // D-shape trigger
            p.mouthOpenH = 10;
            p.mouthW = 12;
            p.blushR = 5;         // Use standard blush instead of custom marks
            p.bounce = -3;        // Big bounce
            break;

        case Emotion::STARSTRUCK:
            // Heart-shaped eyes with big open D-shaped smile and blush
            p.heartEyes = true;
            p.eyeW = 11;
            p.eyeH = 11;
            p.eyeLY = 25;
            p.eyeRY = 25;
            p.mouthCurve = 5;     // D-shape trigger
            p.mouthOpenH = 10;
            p.mouthW = 12;
            p.blushR = 5;
            p.bounce = -2;
            break;


        default:
            break;
    }
    return p;
}

// --- Lifecycle ---

