#pragma once

#include <Arduino.h>
#include "display_config.h"
#include "emotion/emotion_engine.h"

// Plays a personalized greeting animation on boot using the real EmotionEngine.
// If userName is empty, shows a prompt to set the name.
// hour is the current NTP hour (0-23), or -1 if NTP hasn't synced.
void playGreeting(DisplayType& display, EmotionEngine& emotionEngine, const String& userName, int hour);
