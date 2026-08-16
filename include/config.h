#pragma once

// =============================================================
// Minimello Portable — Hardware & Firmware Configuration
// =============================================================

// --- Pin Definitions ---
#define PIN_SDA 8         // I2C SDA → OLED
#define PIN_SCL 10        // I2C SCL → OLED
#define PIN_TOUCH 2       // TTP223 output (active HIGH)
#define PIN_BATTERY_ADC 0 // Battery voltage divider midpoint

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
  false // Set to true if battery & voltage divider are present
#define BATTERY_DIVIDER_RATIO 2.0f  // 100kΩ / 100kΩ voltage divider
#define BATTERY_FULL_MV 4200        // 4.20V = 100%
#define BATTERY_EMPTY_MV 3300       // 3.30V = 0%
#define BATTERY_LOW_PERCENT 10      // Low battery warning threshold
#define BATTERY_CRITICAL_PERCENT 5  // Critical battery threshold
#define BATTERY_READ_INTERVAL 30000 // Read every 30 seconds (ms)
#define BATTERY_SAMPLE_COUNT 8      // ADC samples to average

// --- Touch Input ---
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
#define WIFI_CONNECT_TIMEOUT_MS 15000 // WiFi connection timeout
#define NTP_SERVER "pool.ntp.org"
#define NTP_SYNC_INTERVAL_MS (6ULL * 3600 * 1000) // Resync every 6 hours
#define DEFAULT_TZ_OFFSET 19800                   // IST (+5:30) in seconds

// --- Weather ---
#define WEATHER_REFRESH_MS (30UL * 60 * 1000) // Refresh every 30 minutes
#define WEATHER_API_BASE "http://api.openweathermap.org/data/2.5/weather"

// --- OTA ---
#define OTA_GITHUB_OWNER "Debojit-mitra"
#define OTA_GITHUB_REPO "minimello-portable"
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
