#pragma once
#include <Arduino.h>

// =============================================================
// Minimello Portable — Global Serial Logger
// =============================================================

#define LOG_E(module, format, ...) Serial.printf("[ERROR][%s] %lu: " format "\n", module, millis(), ##__VA_ARGS__)
#define LOG_W(module, format, ...) Serial.printf("[WARN][%s] %lu: " format "\n", module, millis(), ##__VA_ARGS__)
#define LOG_I(module, format, ...) Serial.printf("[INFO][%s] %lu: " format "\n", module, millis(), ##__VA_ARGS__)
#define LOG_D(module, format, ...) Serial.printf("[DEBUG][%s] %lu: " format "\n", module, millis(), ##__VA_ARGS__)
