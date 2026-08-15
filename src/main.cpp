#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "display_config.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include "font_config.h"

#include "config.h"
#include "version.h"
#include "config_manager.h"
#include "touch_manager.h"
#include "power_manager.h"
#include "emotion_engine.h"
#include "clock_engine.h"
#include "screen_manager.h"
#include "network_manager.h"
#include "web_server.h"
#include "weather_service.h"
#include "ota_manager.h"
#include "logger.h"

// =============================================================
// Minimello Portable — Main Entry Point
// =============================================================

// --- Global instances ---
DisplayType display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ConfigManager    configMgr;
TouchManager     touchMgr;
PowerManager     powerMgr;
EmotionEngine    emotionEngine;
ClockEngine      clockEngine;
ScreenManager    screenMgr;
NetworkManager   networkMgr;
MiniWebServer    webServer;
WeatherService   weatherSvc;
OTAManager       otaMgr;
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// --- Timing ---
uint32_t lastFrameMs = 0;
uint32_t lastSlowUpdateMs = 0;
uint32_t lastInteractionMs = 0;  // Tracks last touch for night-mode idle detection

// =============================================================
// Boot Animation
// =============================================================

void drawBootSplash() {
    display.clearDisplay();

    // "MiniMello" branding with custom font
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(1);
    display.setTextColor(DISPLAY_WHITE);

    // Center the text
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(FIRMWARE_NAME, 0, 0, &tbx, &tby, &tbw, &tbh);
    int16_t tx = (SCREEN_WIDTH - tbw) / 2 - tbx;
    int16_t ty = 24;  // Baseline for 12pt font

    display.setCursor(tx, ty);
    display.print(FIRMWARE_NAME);

    // Version text below (smooth U8g2 font)
    char verBuf[16];
    snprintf(verBuf, sizeof(verBuf), "v%s", FIRMWARE_VERSION);
    u8g2Fonts.setFont(FONT_SMALL);
    int16_t vbw = u8g2Fonts.getUTF8Width(verBuf);
    u8g2Fonts.setCursor((SCREEN_WIDTH - vbw) / 2, 38);
    u8g2Fonts.print(verBuf);

    display.display();
}

void drawBootProgress(uint8_t step, uint8_t totalSteps, const char* label) {
    // Progress bar area
    int16_t barX = 14;
    int16_t barY = 42;
    int16_t barW = 100;
    int16_t barH = 8;

    // Clear progress area only
    display.fillRect(0, 38, SCREEN_WIDTH, 26, DISPLAY_BLACK);

    // Progress bar outline
    display.drawRoundRect(barX, barY, barW, barH, 3, DISPLAY_WHITE);

    // Progress bar fill
    int16_t fillW = (int16_t)((barW - 4) * step / totalSteps);
    if (fillW > 0) {
        display.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, DISPLAY_WHITE);
    }

    // Label below progress bar (smooth U8g2 font)
    u8g2Fonts.setFont(FONT_SMALL);
    int16_t lbw = u8g2Fonts.getUTF8Width(label);
    u8g2Fonts.setCursor((SCREEN_WIDTH - lbw) / 2, 62);
    u8g2Fonts.print(label);

    display.display();
}

// =============================================================
// Touch Event Handler
// =============================================================

void onTouchEvent(TouchEvent event) {
    lastInteractionMs = millis();  // Reset idle timer on any touch
    screenMgr.resetAutoSwitchTimer();  // Pause/reset auto-switch timer on any touch interaction

    switch (event) {
        case TouchEvent::TAP:
            LOG_D("TOUCH", "TAP");
            if (screenMgr.getActiveEngine() == ActiveEngine::EMOTION) {
                // React to touch with emotion
                screenMgr.emotionEngine()->onTouch();
            } else {
                // Toggle between Time and Weather manually
                screenMgr.clockEngine()->toggleSubScreen();
            }
            break;

        case TouchEvent::LONG_PRESS:
            LOG_D("TOUCH", "LONG_PRESS");
            screenMgr.switchEngine();
            break;

        case TouchEvent::DOUBLE_TAP:
            LOG_D("TOUCH", "DOUBLE_TAP");
            // Reserved for future use (e.g., toggle debug info)
            break;

        default:
            break;
    }
}

// =============================================================
// Setup
// =============================================================

void setup() {
    Serial.begin(115200);
    LOG_I("SYSTEM", "MiniMello Booting...");

    // --- Step 0: Display init ---
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(I2C_CLOCK_HZ);

    if (!DISPLAY_BEGIN(display, OLED_I2C_ADDR, OLED_RESET)) {
        LOG_E("DISPLAY", "OLED init FAILED");
        for (;;) delay(1000);  // Halt if no display
    }

    display.setRotation(0);
    display.clearDisplay();
    display.display();

    // --- Init U8g2 font renderer ---
    u8g2Fonts.begin(display);
    u8g2Fonts.setFontMode(1);             // Transparent background
    u8g2Fonts.setForegroundColor(DISPLAY_WHITE);

    // --- Boot splash ---
    drawBootSplash();
    delay(BOOT_SPLASH_DURATION_MS);

    // --- Step 1: Config ---
    drawBootProgress(1, BOOT_PROGRESS_STEPS, "Loading config...");
    configMgr.begin();
    delay(100);

    // --- Step 2: Battery ---
#if ENABLE_BATTERY_MODULE
    drawBootProgress(2, BOOT_PROGRESS_STEPS, "Battery check...");
#endif
    powerMgr.begin(PIN_BATTERY_ADC); // Must still run to set internal state
#if ENABLE_BATTERY_MODULE
    LOG_I("POWER", "Battery: %.2fV (%d%%)", powerMgr.getBatteryVoltage(), powerMgr.getBatteryPercent());
    delay(100);
#endif

    // --- Step 3: Touch ---
    drawBootProgress(3, BOOT_PROGRESS_STEPS, "Touch sensor...");
    touchMgr.begin(PIN_TOUCH);
    touchMgr.onEvent(onTouchEvent);
    delay(100);

    // --- Step 4: Engines ---
    drawBootProgress(4, BOOT_PROGRESS_STEPS, "Starting engines...");
    emotionEngine.begin();
    clockEngine.begin();
    clockEngine.setFace((ClockFace)configMgr.clockFace);
    screenMgr.begin(&emotionEngine, &clockEngine);
    screenMgr.setAutoSwitch(configMgr.autoSwitch);
    screenMgr.setAutoSwitchInterval(configMgr.switchIntervalS);
    screenMgr.setClockDuration(configMgr.clockDurationS);

    if (configMgr.defaultEngine == 1) {
        screenMgr.setEngine(ActiveEngine::CLOCK);
    }
    delay(100);

    // --- Step 5: WiFi ---
    drawBootProgress(5, BOOT_PROGRESS_STEPS, "Connecting WiFi...");
    LOG_I("WIFI", "SSID from config: '%s'", configMgr.wifiSSID.c_str());
    networkMgr.begin(configMgr.wifiSSID, configMgr.wifiPass, configMgr.tzOffset);

    // Wait for connection (up to 15s during boot)
    uint32_t wifiStart = millis();
    while (networkMgr.getState() == NetState::CONNECTING &&
           millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
        networkMgr.update(millis());
        if ((millis() - wifiStart) % 2000 < 100) {
            LOG_I("WIFI", "Status: %d", WiFi.status());
        }
        delay(100);
    }
    LOG_I("WIFI", "Final state: %d", (int)networkMgr.getState());

    // --- Step 6: Web Server ---
    drawBootProgress(6, BOOT_PROGRESS_STEPS, "Web server...");
    webServer.begin(&configMgr, &screenMgr, &powerMgr, &networkMgr);
    if (networkMgr.getState() == NetState::AP_MODE) {
        webServer.enableCaptivePortal();
    }
    delay(100);

    // --- Step 7: Weather ---
    drawBootProgress(7, BOOT_PROGRESS_STEPS, "Weather data...");
    weatherSvc.begin(configMgr.weatherApiKey, configMgr.weatherCity);
    delay(100);

    // --- Step 8: OTA ---
    drawBootProgress(8, BOOT_PROGRESS_STEPS, "Ready!");
    otaMgr.begin();
    delay(300);

    // --- Set display brightness ---
    DISPLAY_SETCONTRAST(display, configMgr.brightness);

    // --- Configure mood system ---
    emotionEngine.setMoodInterval(configMgr.moodIntervalS);

    // --- Ready ---
    lastFrameMs = millis();
    lastSlowUpdateMs = millis();
    lastInteractionMs = millis();  // Don't trigger idle sleep immediately
    LOG_I("SYSTEM", "MiniMello Ready");

    // Check wakeup reason
    if (powerMgr.wasWokenByTouch()) {
        LOG_I("SYSTEM", "Woke from deep sleep: touch");
    } else if (powerMgr.wasWokenByTimer()) {
        LOG_I("SYSTEM", "Woke from deep sleep: timer");
    }
}

// =============================================================
// Main Loop
// =============================================================

void loop() {
    uint32_t now = millis();

    // --- Always update touch (high frequency) ---
    touchMgr.update();

    // --- Apply brightness immediately when changed via WebUI ---
    if (configMgr.brightnessChanged) {
        configMgr.brightnessChanged = false;
        DISPLAY_SETCONTRAST(display, configMgr.brightness);
    }

    // --- Apply weather credentials when changed via WebUI ---
    if (configMgr.weatherChanged) {
        configMgr.weatherChanged = false;
        weatherSvc.setCredentials(configMgr.weatherApiKey, configMgr.weatherCity);
    }

    // --- Slow updates (every 1 second) ---
    if (now - lastSlowUpdateMs >= 1000) {
        lastSlowUpdateMs = now;

        // Battery
        powerMgr.update(now);

        // Network
        networkMgr.update(now);

        // Weather
        weatherSvc.update(now);

        // OTA
        otaMgr.update(now);

        // Update clock data
        struct tm timeinfo;
        if (networkMgr.getLocalTime(timeinfo)) {
            clockEngine.setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            clockEngine.setDate(timeinfo.tm_mday, timeinfo.tm_mon + 1,
                                timeinfo.tm_year + 1900, timeinfo.tm_wday);

            // Calculate if we are in night hours based on user config
            bool isNight = false;
            if (configMgr.nightModeEnabled) {
                if (configMgr.nightStartHour > configMgr.nightEndHour) {
                    isNight = (timeinfo.tm_hour >= configMgr.nightStartHour || timeinfo.tm_hour < configMgr.nightEndHour);
                } else {
                    isNight = (timeinfo.tm_hour >= configMgr.nightStartHour && timeinfo.tm_hour < configMgr.nightEndHour);
                }
            }

            // Time-based emotion (Sleepy during night hours)
            emotionEngine.checkTimeBasedMood(timeinfo.tm_hour, isNight);

            // Night mode deep sleep check
            // Guards:
            //  1. NTP must have synced (avoid stale RTC time causing boot loops)
            //  2. User must be idle for NIGHT_MODE_IDLE_MS (don't sleep during active use)
            if (isNight &&
                networkMgr.isTimeSynced() &&
                (now - lastInteractionMs) > NIGHT_MODE_IDLE_MS) {

                LOG_I("SYSTEM", "Night mode: entering deep sleep");

                // Show sleep notification to user
                    display.clearDisplay();

                    // Moon icon (hand-drawn with circles)
                    display.fillCircle(64, 24, 10, DISPLAY_WHITE);
                    display.fillCircle(58, 20, 10, DISPLAY_BLACK);  // Crescent cutout

                    // "Sleeping..." text
                    u8g2Fonts.setFont(FONT_MEDIUM);
                    const char* sleepMsg = "Sleeping...";
                    int16_t sw = u8g2Fonts.getUTF8Width(sleepMsg);
                    u8g2Fonts.setCursor((SCREEN_WIDTH - sw) / 2, 52);
                    u8g2Fonts.print(sleepMsg);

                    // "Touch to wake" hint
                    u8g2Fonts.setFont(FONT_SMALL);
                    const char* hintMsg = "Touch to wake";
                    int16_t hw = u8g2Fonts.getUTF8Width(hintMsg);
                    u8g2Fonts.setCursor((SCREEN_WIDTH - hw) / 2, 63);
                    u8g2Fonts.print(hintMsg);

                    display.display();
                    delay(2000);  // Let user see the message

                    display.clearDisplay();
                    display.display();
                    powerMgr.enterDeepSleep(NIGHT_WAKE_CHECK_US);
                    // Never reaches here — device reboots on wake
                }
            }
        // Update clock with battery/wifi/ip status
        clockEngine.setBattery(powerMgr.getBatteryPercent(), false);
        clockEngine.setWiFi(networkMgr.isConnected(), networkMgr.getRSSI());
        clockEngine.setIP(networkMgr.getIP());
        clockEngine.setWeather(weatherSvc.getData());

        // Pass weather condition to emotion engine for subtle mood bias
        const WeatherData& wd = weatherSvc.getData();
        if (wd.valid) {
            emotionEngine.setWeatherCondition(wd.condition);
        }

#if ENABLE_BATTERY_MODULE
        // Low battery warning
        if (powerMgr.isCriticalBattery()) {
            LOG_E("POWER", "CRITICAL battery! Entering deep sleep.");
            display.clearDisplay();
            u8g2Fonts.setFont(FONT_MEDIUM);
            const char* lowBattMsg = "LOW BATTERY";
            int16_t lbw = u8g2Fonts.getUTF8Width(lowBattMsg);
            u8g2Fonts.setCursor((SCREEN_WIDTH - lbw) / 2, 36);
            u8g2Fonts.print(lowBattMsg);
            display.display();
            delay(2000);
            display.clearDisplay();
            display.display();
            powerMgr.enterDeepSleep(0);  // Sleep until touch
        }
#endif
    }

    // --- Frame-rate limited rendering ---
    uint32_t frameInterval = screenMgr.getFrameInterval();
    uint32_t deltaMs = now - lastFrameMs;

    if (deltaMs >= frameInterval) {
        lastFrameMs = now;

        // Cap delta to prevent jumps after long operations
        if (deltaMs > 200) deltaMs = 200;

        // Update active engine
        screenMgr.update(deltaMs);

        // Render
        display.clearDisplay();
        screenMgr.render(display);
        display.display();
    }

    yield();  // Feed watchdog, let WiFi stack run
}
