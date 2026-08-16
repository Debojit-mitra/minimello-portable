#include "display_config.h"
#include "font_config.h"
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "clock/clock_engine.h"
#include "config.h"
#include "config/config_manager.h"
#include "emotion/emotion_engine.h"
#include "logger.h"
#include "network/network_manager.h"
#include "ota/ota_manager.h"
#include "power/power_manager.h"
#include "screen/screen_manager.h"
#include "server/web_server.h"
#include "touch/touch_manager.h"
#include "ui/boot_animation.h"
#include "ui/ui_components.h"
#include "ui/greeting.h"
#include "version.h"
#include "weather/weather_service.h"
#include <WiFi.h>

// =============================================================
// Minimello Portable — Main Entry Point
// =============================================================

// --- Global instances ---
DisplayType display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ConfigManager configMgr;
TouchManager touchMgr;
PowerManager powerMgr;
EmotionEngine emotionEngine;
ClockEngine clockEngine;
ScreenManager screenMgr;
NetworkManager networkMgr;
MiniWebServer webServer;
WeatherService weatherSvc;
OTAManager otaMgr;
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// --- Timing ---
uint32_t lastFrameMs = 0;
uint32_t lastSlowUpdateMs = 0;
uint32_t lastInteractionMs =
    0; // Tracks last touch for night-mode idle detection
bool isScreenSleeping = false;

// =============================================================
// Boot animation is handled in ui/boot_animation.cpp

void drawOtaProgress(int percent) {
  // Clear entire screen for clean OTA mode
  display.clearDisplay();

  // Vertically centered progress bar
  int16_t barW = 100;
  int16_t barH = 8;
  int16_t barX = (SCREEN_WIDTH - barW) / 2;
  int16_t barY = (SCREEN_HEIGHT - barH) / 2 - 6; // slightly above center

  // Progress bar outline
  display.drawRoundRect(barX, barY, barW, barH, 3, DISPLAY_WHITE);

  // Progress bar fill
  int16_t fillW = (int16_t)((barW - 4) * percent / 100);
  if (fillW > 0) {
    display.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2,
                          DISPLAY_WHITE);
  }

  // Label below progress bar
  char label[32];
  snprintf(label, sizeof(label), "Updating: %d%%", percent);
  u8g2Fonts.setFont(FONT_MEDIUM);
  int16_t lbw = u8g2Fonts.getUTF8Width(label);
  u8g2Fonts.setCursor((SCREEN_WIDTH - lbw) / 2, barY + barH + 16);
  u8g2Fonts.print(label);
  display.display();
}

void onTouchEvent(TouchEvent event) {
  lastInteractionMs = millis();     // Reset idle timer on any touch
  screenMgr.resetAutoSwitchTimer(); // Pause/reset auto-switch timer on any
                                    // touch interaction

  if (isScreenSleeping) {
    LOG_I("SYSTEM", "Woke from screensaver");
    isScreenSleeping = false;
    // Restore brightness
    DISPLAY_SETCONTRAST(display, configMgr.brightness);
    return; // Consume touch to just wake up
  }

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

  case TouchEvent::VERY_LONG_PRESS:
    LOG_W("TOUCH", "VERY_LONG_PRESS - Hard Restarting!");
    WiFi.disconnect(true, true);
    delay(1000);
    ESP.restart();
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

bool g_isSetupMode = false;

void setup() {
  Serial.begin(115200);
  LOG_I("SYSTEM", "MiniMello Booting...");

  // --- Load config first so we know if we're in setup mode ---
  configMgr.begin();
  g_isSetupMode = (configMgr.wifiSSID.length() == 0);
  delay(100);

  // --- Step 0: Display init ---
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  if (!DISPLAY_BEGIN(display, OLED_I2C_ADDR, OLED_RESET)) {
    LOG_E("DISPLAY", "OLED init FAILED");
    for (;;)
      delay(1000); // Halt if no display
  }

  display.setRotation(0);
  display.clearDisplay();
  display.display();

  // --- Init U8g2 font renderer ---
  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1); // Transparent background
  u8g2Fonts.setForegroundColor(DISPLAY_WHITE);

  // --- Boot splash ---
  playBootAnimation();
  // delay(BOOT_SPLASH_DURATION_MS); // Now handled by the animation itself

  // --- Step 1: Config ---
  drawBootProgress(1, BOOT_PROGRESS_STEPS, "Loading config...");
  // configMgr.begin(); -> Moved to very top!

  // --- Step 2: Battery ---
#if ENABLE_BATTERY_MODULE
  drawBootProgress(2, BOOT_PROGRESS_STEPS, "Battery check...");
#endif
  powerMgr.begin(PIN_BATTERY_ADC); // Must still run to set internal state
#if ENABLE_BATTERY_MODULE
  LOG_I("POWER", "Battery: %.2fV (%d%%)", powerMgr.getBatteryVoltage(),
        powerMgr.getBatteryPercent());
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
  if (configMgr.wifiSSID.length() > 0) {
    drawBootProgress(5, BOOT_PROGRESS_STEPS, "Connecting WiFi...");
  } else {
    drawBootProgress(5, BOOT_PROGRESS_STEPS, "Starting hotspot...");
  }
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
  if (networkMgr.getState() == NetState::AP_MODE) {
    drawBootProgress(6, BOOT_PROGRESS_STEPS, "Preparing setup...");
  } else {
    drawBootProgress(6, BOOT_PROGRESS_STEPS, "Web server...");
  }
  webServer.begin(&configMgr, &screenMgr, &powerMgr, &networkMgr);
  if (networkMgr.getState() == NetState::AP_MODE) {
    webServer.enableCaptivePortal();
  }
  delay(100);


  // If we are in AP mode, stay in the setup screen until connected!
  if (networkMgr.getState() == NetState::AP_MODE) {
    runSetupScreen(display, networkMgr, webServer, touchMgr);
  }

  // --- Step 7: Weather ---
  drawBootProgress(7, BOOT_PROGRESS_STEPS, "Weather data...");
  
  if (configMgr.weatherCity.isEmpty() && networkMgr.isConnected()) {
      drawBootProgress(7, BOOT_PROGRESS_STEPS, "Locating...");
      HTTPClient http;
      http.begin("http://ip-api.com/json/");
      http.setTimeout(4000);
      if (http.GET() == HTTP_CODE_OK) {
          JsonDocument doc;
          if (!deserializeJson(doc, http.getString())) {
              configMgr.weatherLat = doc["lat"].as<float>();
              configMgr.weatherLon = doc["lon"].as<float>();
              configMgr.weatherCity = doc["city"].as<String>();
              configMgr.save(); // Save to NVS
              LOG_I("SYSTEM", "Auto-detected location: %s", configMgr.weatherCity.c_str());
          }
      }
      http.end();
  }

  weatherSvc.begin(configMgr.weatherLat, configMgr.weatherLon, configMgr.weatherCity);
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
  lastInteractionMs = millis(); // Don't trigger idle sleep immediately
  LOG_I("SYSTEM", "MiniMello Ready");

  // --- Greeting animation (cold boot only) ---
  if (millis() < 30000) {
    int greetHour = -1;
    struct tm timeinfo;
    if (networkMgr.isTimeSynced() && networkMgr.getLocalTime(timeinfo)) {
      greetHour = timeinfo.tm_hour;
    }
    playGreeting(display, emotionEngine, configMgr.userName, greetHour);
  }

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
    weatherSvc.setCredentials(configMgr.weatherLat, configMgr.weatherLon, configMgr.weatherCity);
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
          isNight = (timeinfo.tm_hour >= configMgr.nightStartHour ||
                     timeinfo.tm_hour < configMgr.nightEndHour);
        } else {
          isNight = (timeinfo.tm_hour >= configMgr.nightStartHour &&
                     timeinfo.tm_hour < configMgr.nightEndHour);
        }
      }

      // Time-based emotion (Sleepy during night hours)
      emotionEngine.checkTimeBasedMood(timeinfo.tm_hour, isNight);

      // Night mode deep sleep check
      // Guards:
      //  1. NTP must have synced (avoid stale RTC time causing boot loops)
      //  2. User must be idle for NIGHT_MODE_IDLE_MS (don't sleep during active
      //  use)
      if (isNight && networkMgr.isTimeSynced() &&
          (now - lastInteractionMs) > NIGHT_MODE_IDLE_MS) {

        if (!isScreenSleeping) {
#if ENABLE_BATTERY_MODULE
          LOG_I("SYSTEM", "Night mode: entering deep sleep");
#else
          LOG_I("SYSTEM", "Night mode: entering screensaver");
#endif

          // Show sleep notification to user
          display.clearDisplay();
          display.fillCircle(64, 24, 10, DISPLAY_WHITE);
          display.fillCircle(58, 20, 10, DISPLAY_BLACK); // Crescent cutout

          u8g2Fonts.setFont(FONT_MEDIUM);
          const char *sleepMsg = "Sleeping...";
          int16_t sw = u8g2Fonts.getUTF8Width(sleepMsg);
          u8g2Fonts.setCursor((SCREEN_WIDTH - sw) / 2, 52);
          u8g2Fonts.print(sleepMsg);

          u8g2Fonts.setFont(FONT_SMALL);
          const char *hintMsg = "Touch to wake";
          int16_t hw = u8g2Fonts.getUTF8Width(hintMsg);
          u8g2Fonts.setCursor((SCREEN_WIDTH - hw) / 2, 63);
          u8g2Fonts.print(hintMsg);

          display.display();
          delay(2000); // Let user see the message

          display.clearDisplay();
          display.display();

#if ENABLE_BATTERY_MODULE
          powerMgr.enterDeepSleep(NIGHT_WAKE_CHECK_US);
#else
          // Soft sleep for USB power (keeps WiFi and OTA alive)
          DISPLAY_SETCONTRAST(display, 0); // Hardware display off
          isScreenSleeping = true;
#endif
        }
      }
    }
    // Update clock with battery/wifi/ip status
    clockEngine.setBattery(powerMgr.getBatteryPercent(), false);
    clockEngine.setWiFi(networkMgr.isConnected(), networkMgr.getRSSI());
    clockEngine.setIP(networkMgr.getIP());
    clockEngine.setWeather(weatherSvc.getData());

    // Pass weather condition to emotion engine for subtle mood bias
    const WeatherData &wd = weatherSvc.getData();
    if (wd.valid) {
      emotionEngine.setWeatherCondition(wd.condition);
    }

#if ENABLE_BATTERY_MODULE
    // Low battery warning
    if (powerMgr.isCriticalBattery()) {
      LOG_E("POWER", "CRITICAL battery! Entering deep sleep.");
      display.clearDisplay();
      u8g2Fonts.setFont(FONT_MEDIUM);
      const char *lowBattMsg = "LOW BATTERY";
      int16_t lbw = u8g2Fonts.getUTF8Width(lowBattMsg);
      u8g2Fonts.setCursor((SCREEN_WIDTH - lbw) / 2, 36);
      u8g2Fonts.print(lowBattMsg);
      display.display();
      delay(2000);
      display.clearDisplay();
      display.display();
      powerMgr.enterDeepSleep(0); // Sleep until touch
    }
#endif
  }

  // --- Frame-rate limited rendering ---
  uint32_t frameInterval = screenMgr.getFrameInterval();
  uint32_t deltaMs = now - lastFrameMs;

  webServer.update();

  // --- Render frame ---
  if (!isScreenSleeping && deltaMs >= frameInterval) {
    lastFrameMs = now;

    // Cap delta to prevent jumps after long operations
    if (deltaMs > 200)
      deltaMs = 200;

    // Update active engine
    screenMgr.update(deltaMs);

    // Render
    display.clearDisplay();
    screenMgr.render(display);

    // Overlay Reboot Warning
    uint32_t holdTime = touchMgr.getHoldTimeMs();
    if (holdTime > 3000) {
      uint8_t progress = (holdTime - 3000) * 100 / (TOUCH_VERY_LONG_PRESS_MS - 3000);
      UIComponents::drawRestartWarning(display, progress, true);
    }

    display.display();
  }

  yield(); // Feed watchdog, let WiFi stack run
}
