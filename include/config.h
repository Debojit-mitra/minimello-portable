#pragma once

// =============================================================
// Minimello Portable — Hardware & Firmware Configuration
// =============================================================

// --- Pin Definitions ---
#define PIN_SDA 8         // I2C SDA → OLED
#define PIN_SCL 10        // I2C SCL → OLED
#define PIN_TOUCH 2       // TTP223 output (active HIGH)
#define PIN_BATTERY_ADC 4 // Battery voltage divider midpoint
#define PIN_CHRG 5        // TP4056 CHRG pin (LOW = charging)

// --- OLED Display ---
#define USE_1_3_INCH_OLED                                                      \
  false // Set to true for 1.3" (SH1106), false for 0.96" (SSD1306)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_I2C_ADDR 0x3C
#define OLED_RESET -1       // No hardware reset pin
#define I2C_CLOCK_HZ 400000 // 400kHz I2C (SSD1306 max spec)

// --- Battery ---
#define ENABLE_BATTERY_MODULE                                                  \
  true // Set to true if battery & voltage divider are present
#define BATTERY_DIVIDER_RATIO 2.018f  // Calibrated: increased from 2.018 to compensate for 3.60V false-triggers
#define BATTERY_FULL_MV 4200        // 4.20V = 100%
#define BATTERY_EMPTY_MV 3350       // 3.35V = 0% (headroom above ESP32-C3 brownout)
#define BATTERY_LOW_PERCENT 10      // Low battery warning threshold
#define BATTERY_CRITICAL_PERCENT 5  // Critical battery threshold
#define BATTERY_READ_INTERVAL 30000 // Read every 30 seconds (ms)
#define BATTERY_SAMPLE_COUNT 8      // ADC samples to average

// --- Battery Power Optimizations ---
// These only take effect when ENABLE_BATTERY_MODULE is true.
#define ENABLE_CPU_SCALING true      // Drop to 80MHz when WiFi radio is off
#define CPU_FREQ_HIGH 160            // MHz — when WiFi is active
#define CPU_FREQ_LOW 80              // MHz — when WiFi is off (saves ~20mA)
#define ENABLE_LIGHT_SLEEP true      // Use esp_light_sleep between frames

// --- Touch Input ---
#define ENABLE_TOUCH_SENSOR true // Set to true when TTP223 is connected
#define TOUCH_DEBOUNCE_MS 50     // Standard debounce
#define TOUCH_LONG_PRESS_MS 1500 // Long press threshold
#define TOUCH_VERY_LONG_PRESS_MS                                               \
  5000 // 5 seconds to restart (TTP223 auto-calibrates at ~8s)
#define TOUCH_DOUBLE_TAP_MS 400 // Max gap between double-tap

// --- Screen Manager ---
#define AUTO_SWITCH_INTERVAL_S 30   // Default: switch engine every 30s
#define CLOCK_DISPLAY_DURATION_S 10 // Default: show clock for 10s
#define TRANSITION_DURATION_MS 200  // Slide transition duration

// --- Emotion Engine ---
#define EMOTION_FPS_ACTIVE 20        // FPS during animation/transition
#define EMOTION_FPS_IDLE 10          // FPS during idle
#define BLINK_INTERVAL_MIN_MS 3000   // Min time between blinks
#define BLINK_INTERVAL_MAX_MS 7000   // Max time between blinks
#define BLINK_DURATION_MS 180        // Total blink duration (close + open)
#define EMOTION_LERP_SPEED 5.0f      // Interpolation speed for transitions
#define TOUCH_REACTION_DURATION 3000 // How long touch reaction lasts (ms)

// --- Clock Engine ---
#define CLOCK_FPS 2 // FPS for clock display (save power)

// --- Night Mode (Deep Sleep) ---
#define NIGHT_START_HOUR 23                        // Default night start
#define NIGHT_END_HOUR 7                           // Default night end
#define NIGHT_WAKE_CHECK_US (30ULL * 60 * 1000000) // Wake every 30min to check
#define NIGHT_MODE_IDLE_MS 300000 // Sleep after 5min idle during night hours

// --- Network ---
#define WIFI_AP_PREFIX "Minimello-"
#define WIFI_AP_PASSWORD "minimello"  // Setup AP password (shown on screen)
#define WIFI_CONNECT_TIMEOUT_MS 10000 // WiFi connection timeout (fail fast to trigger retry)
#define NTP_SERVER "pool.ntp.org"
#define NTP_SYNC_INTERVAL_MS (6ULL * 3600 * 1000) // Resync every 6 hours
#define DEFAULT_TZ_OFFSET 19800                   // IST (+5:30) in seconds

// --- WiFi Power Management (Duty Cycling) ---
// When enabled, WiFi radio is powered OFF when idle and woken on-demand
// by services (weather, NTP, OTA) or user interaction (touch).
#define WIFI_DUTY_CYCLE true            // Enable demand-driven WiFi sleep
#define WIFI_ACTIVE_WINDOW_MS  30000    // 30s: min time to stay on after wake
#define WIFI_WEB_IDLE_TIMEOUT_MS 180000 // 3 min: stay on while web UI is active

// --- Weather ---
#define WEATHER_REFRESH_MS 1800000 // 30 minutes
#define WEATHER_API_BASE "http://api.open-meteo.com/v1/forecast"

// --- OTA ---
#define OTA_GITHUB_OWNER "Debojit-mitra"
#define OTA_GITHUB_REPO "minimello-portable-releases"
#define OTA_CHECK_INTERVAL_MS (6ULL * 3600 * 1000) // Check every 6 hours
#define OTA_API_URL                                                            \
  "https://api.github.com/repos/" OTA_GITHUB_OWNER "/" OTA_GITHUB_REPO         \
  "/releases/latest"

// --- Boot Animation ---
#define BOOT_SPLASH_DURATION_MS 1500 // How long to show splash before progress
#define BOOT_PROGRESS_STEPS 8        // Number of init steps in progress bar

// --- Hardware Notes ---
// GPIO2 is a strapping pin on ESP32-C3. TTP223 (active-HIGH) could
// theoretically interfere with boot if touched during reset.
// TODO: Add 10kΩ pull-down resistor on GPIO2 for production units.
