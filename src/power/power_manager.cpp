#include "power/power_manager.h"
#include "config.h"
#include "logger.h"
#include <esp_sleep.h>

// RTC-retained last-known voltage — survives deep sleep, cleared on power cycle
RTC_DATA_ATTR static float _rtcLastVoltage = 0.0f;

// =============================================================
// PowerManager — Implementation
// =============================================================

// LiPo discharge curve lookup table (non-linear)
// Maps voltage (mV) to percentage
static const struct {
    uint16_t voltage;
    uint8_t  percent;
} DISCHARGE_CURVE[] = {
    { 4200, 100 },
    { 4150,  97 },
    { 4110,  94 },
    { 4080,  90 },
    { 4020,  85 },
    { 3980,  80 },
    { 3950,  75 },
    { 3910,  70 },
    { 3870,  65 },
    { 3840,  60 },
    { 3820,  55 },
    { 3800,  50 },
    { 3790,  45 },
    { 3770,  40 },
    { 3750,  35 },
    { 3730,  30 },
    { 3700,  25 },
    { 3670,  20 },
    { 3630,  15 },
    { 3580,  10 },
    { 3490,   5 },
    { 3350,   0 },
};
static const int CURVE_SIZE = sizeof(DISCHARGE_CURVE) / sizeof(DISCHARGE_CURVE[0]);

void PowerManager::begin(uint8_t adcPin, uint8_t chrgPin) {
    _adcPin = adcPin;
    _chrgPin = chrgPin;
#if ENABLE_BATTERY_MODULE
    analogSetAttenuation(ADC_11db);  // Full range (~0-2.6V input)
    pinMode(_adcPin, INPUT);
    pinMode(_chrgPin, INPUT_PULLUP); // TP4056 CHRG is open-drain, needs pullup

    // Wait a full 1 second for the battery voltage to stabilize under boot load
    // and for hardware capacitors to charge before we trust the ADC.
    delay(1000);

    // Prime the ADC (discard first few reads which are often inaccurate after init)
    for (int i = 0; i < 10; i++) {
        analogReadMilliVolts(_adcPin);
        delay(2);
    }

    // Average multiple readings over ~1 second for stable boot voltage
    float bootSum = 0.0f;
    const int BOOT_SAMPLES = 20;
    for (int i = 0; i < BOOT_SAMPLES; i++) {
        bootSum += readRawVoltage();
        delay(50);
    }
    _rawVoltage = bootSum / BOOT_SAMPLES;
    _voltage = _rawVoltage;

    // Blend with RTC-retained last-known voltage if plausible
    if (_rtcLastVoltage >= 2.5f && _rtcLastVoltage <= 4.3f) {
        _voltage = (_rtcLastVoltage + _rawVoltage) / 2.0f;
        LOG_I("POWER", "Blended boot voltage: %.2fV (RTC: %.2fV, fresh: %.2fV)",
              _voltage, _rtcLastVoltage, _rawVoltage);
    }

    _rawPercent = voltageToPercent(_voltage);
    _percent = _rawPercent;
    detectPowerSource();
    
    const char* sourceStr = "BATTERY";
    if (_powerSource == PowerSource::CHARGING) sourceStr = "CHARGING (USB+Batt)";
    if (_powerSource == PowerSource::USB_POWERED) sourceStr = "USB ONLY";
    LOG_I("POWER", "Initial power source: %s [V: %.2fV, CHRG_PIN: %d]", sourceStr, _voltage, _chrgPinLow);
#else
    _rawVoltage = 4.2f;
    _voltage = 4.2f;
    _rawPercent = 100;
    _percent = 100;
    _powerSource = PowerSource::USB_POWERED;
#endif
    
    _initialized = true;
    _lastReadMs = millis();
}

void PowerManager::update(uint32_t nowMs) {
#if ENABLE_BATTERY_MODULE
    // Detect power source changes immediately (digitalRead only, no ADC overhead)
    detectPowerSource();

    if (nowMs - _lastReadMs >= BATTERY_READ_INTERVAL) {
        _lastReadMs = nowMs;
        
        _rawVoltage = readRawVoltage();
        
        // EMA smoothing — slower alpha while charging to dampen IR drop overshoot
        // (TP4056 charge current inflates terminal voltage above resting level)
        if (_powerSource == PowerSource::CHARGING) {
            _voltage = (_voltage * 0.95f) + (_rawVoltage * 0.05f);
        } else {
            _voltage = (_voltage * 0.85f) + (_rawVoltage * 0.15f);
        }
        
        // Only persist to RTC when NOT charging (avoids inflated values contaminating reboots)
        if (_powerSource != PowerSource::CHARGING) {
            _rtcLastVoltage = _voltage;
        }
        
        _rawPercent = voltageToPercent(_voltage);
        
        // Charging ratchet: only allow percent to increase while charging
        // On battery: standard ±2% hysteresis to prevent toggling
        if (_powerSource == PowerSource::CHARGING) {
            if (_rawPercent > _percent) {
                _percent = _rawPercent;
            }
        } else {
            if (abs((int)_rawPercent - (int)_percent) >= 2) {
                _percent = _rawPercent;
            }
        }
    }
#endif
}

float PowerManager::getBatteryVoltage() const {
    return _voltage;
}

uint8_t PowerManager::getBatteryPercent() const {
    return _percent;
}




bool PowerManager::isLowBattery() const {
    return _percent <= BATTERY_LOW_PERCENT;
}

bool PowerManager::isCriticalBattery() const {
    // Don't trigger critical battery actions when on USB power
    if (_powerSource != PowerSource::BATTERY) return false;
    return _percent <= BATTERY_CRITICAL_PERCENT;
}

PowerSource PowerManager::getPowerSource() const {
    return _powerSource;
}

bool PowerManager::isCharging() const {
    return _powerSource == PowerSource::CHARGING;
}

bool PowerManager::isOnUSBPower() const {
    return _powerSource == PowerSource::USB_POWERED;
}

void PowerManager::enterDeepSleep(uint64_t wakeupTimerUs) {
    // Configure timer wakeup if requested
    if (wakeupTimerUs > 0) {
        esp_sleep_enable_timer_wakeup(wakeupTimerUs);
    }

    // Configure GPIO2 (TTP223 touch) as wakeup source
    // TTP223 outputs HIGH on touch → wake on HIGH level
    esp_deep_sleep_enable_gpio_wakeup(
        1ULL << PIN_TOUCH,
        ESP_GPIO_WAKEUP_GPIO_HIGH
    );

    // Enter deep sleep (never returns — device reboots on wake)
    esp_deep_sleep_start();
}

bool PowerManager::wasWokenByTouch() const {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
}

bool PowerManager::wasWokenByTimer() const {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

// --- Private ---

float PowerManager::readRawVoltage() {
    // Average multiple ADC samples with outlier rejection for stability
    const int NUM_SAMPLES = 48;
    uint32_t samples[NUM_SAMPLES];
    uint32_t sum = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = analogReadMilliVolts(_adcPin);
        sum += samples[i];
        delayMicroseconds(1000);  // Spread samples over ~48ms
    }

    // Compute rough average, then reject outliers >100mV from it
    float avg = (float)sum / NUM_SAMPLES;
    uint32_t filteredSum = 0;
    int filteredCount = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (abs((int)samples[i] - (int)avg) <= 100) {
            filteredSum += samples[i];
            filteredCount++;
        }
    }
    float adcMv = (filteredCount > 0) ? (float)filteredSum / filteredCount : avg;

    // Apply voltage divider ratio: battery = ADC × 2
    float batteryMv = adcMv * BATTERY_DIVIDER_RATIO;
    return batteryMv / 1000.0f;  // Convert to volts
}

uint8_t PowerManager::voltageToPercent(float voltage) {
    uint16_t mv = (uint16_t)(voltage * 1000);

    // Clamp to range
    if (mv >= DISCHARGE_CURVE[0].voltage) return 100;
    if (mv <= DISCHARGE_CURVE[CURVE_SIZE - 1].voltage) return 0;

    // Linear interpolation between curve points
    for (int i = 0; i < CURVE_SIZE - 1; i++) {
        if (mv >= DISCHARGE_CURVE[i + 1].voltage) {
            float ratio = (float)(mv - DISCHARGE_CURVE[i + 1].voltage) /
                          (float)(DISCHARGE_CURVE[i].voltage - DISCHARGE_CURVE[i + 1].voltage);
            return DISCHARGE_CURVE[i + 1].percent +
                   (uint8_t)(ratio * (DISCHARGE_CURVE[i].percent - DISCHARGE_CURVE[i + 1].percent));
        }
    }
    return 0;
}

// =============================================================
// OLED Power Control
// =============================================================
// The SSD1306/SH1106 charge pump draws ~15mA even with a blank screen.
// Sending DISPLAYOFF shuts down the DC-DC converter and panel driver,
// dropping OLED current to ~5µA.  We use this before deep sleep.

void PowerManager::displayOff(DisplayType& d) {
#if USE_1_3_INCH_OLED
    // SH1106 uses identical command byte
    d.oled_command(SH110X_DISPLAYOFF);
#else
    d.ssd1306_command(SSD1306_DISPLAYOFF);
#endif
    LOG_I("POWER", "OLED charge pump OFF");
}

void PowerManager::displayOn(DisplayType& d, uint8_t brightness) {
#if USE_1_3_INCH_OLED
    d.oled_command(SH110X_DISPLAYON);
#else
    d.ssd1306_command(SSD1306_DISPLAYON);
#endif
    DISPLAY_SETCONTRAST(d, brightness);
    LOG_I("POWER", "OLED charge pump ON (brightness=%u)", brightness);
}

// =============================================================
// CPU Frequency Scaling
// =============================================================
// ESP32-C3 at 160MHz draws ~40mA; at 80MHz ~25mA.
// We drop to 80MHz whenever WiFi is powered off since the OLED I²C
// (400kHz) and touch polling don't need full clock speed.

void PowerManager::setCpuFrequency(uint32_t mhz) {
    uint32_t currentMhz = getCpuFrequencyMhz();
    if (currentMhz != mhz) {
        setCpuFrequencyMhz(mhz);
        LOG_I("POWER", "CPU: %luMHz -> %luMHz", currentMhz, mhz);
    }
}

// =============================================================
// Power Source Detection
// =============================================================
// TP4056 CHRG pin behavior:
//   LOW  = actively charging (USB connected, battery not full)
//   HIGH = not charging (battery-only, charge complete, or no battery)
//
// Combined with voltage reading:
//   CHRG LOW                        → CHARGING (USB + battery)
//   CHRG HIGH + voltage < 2.5V      → USB_POWERED (no battery connected)
//   CHRG HIGH + voltage ≥ 2.5V      → BATTERY (discharging or charge complete)

void PowerManager::detectPowerSource() {
    PowerSource prevSource = _powerSource;
    _chrgPinLow = (digitalRead(_chrgPin) == LOW);

    if (_chrgPinLow) {
        _powerSource = PowerSource::CHARGING;
    } else if (_voltage < 2.5f || _voltage > 4.5f) {
        // CHRG is HIGH but voltage is too low for a real battery (floating ADC)
        // OR voltage is too high for a LiPo (4.5V+ means 5V USB rail is powering the divider)
        _powerSource = PowerSource::USB_POWERED;
    } else {
        _powerSource = PowerSource::BATTERY;
    }

    if (prevSource != _powerSource) {
        // On unplug: reset voltage to real resting level immediately
        if (prevSource == PowerSource::CHARGING && _powerSource == PowerSource::BATTERY) {
            _rawVoltage = readRawVoltage();
            _voltage = _rawVoltage;
            _rawPercent = voltageToPercent(_voltage);
            _percent = _rawPercent;
            _rtcLastVoltage = _voltage;
            LOG_I("POWER", "Unplug reset: %.2fV (%d%%)", _voltage, _percent);
        }

        const char* sourceStr = "BATTERY";
        if (_powerSource == PowerSource::CHARGING) sourceStr = "CHARGING (USB+Batt)";
        if (_powerSource == PowerSource::USB_POWERED) sourceStr = "USB ONLY";
        LOG_I("POWER", "Power source changed to: %s [V: %.2fV, CHRG_PIN: %d]", sourceStr, _voltage, _chrgPinLow);
    }
}

