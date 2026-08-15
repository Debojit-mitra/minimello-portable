#pragma once

#include <Arduino.h>

// =============================================================
// PowerManager — Battery monitoring & deep sleep
// =============================================================
// Reads battery voltage via GPIO0 ADC through 100kΩ/100kΩ divider.
// Uses exponential moving average + hysteresis for stable readings.
// Manages deep sleep with GPIO2 (TTP223) wakeup and timer wakeup.

enum class BatteryLevel : uint8_t {
    BATT_FULL = 0,       // 80-100%
    BATT_GOOD,           // 50-79%
    BATT_MEDIUM,         // 25-49%
    BATT_LOW,            // 10-24%
    BATT_CRITICAL        // 0-9%
};

class PowerManager {
public:
    void begin(uint8_t adcPin);
    void update(uint32_t nowMs);  // Call periodically

    // Battery readings
    float       getBatteryVoltage() const;
    uint8_t     getBatteryPercent() const;
    BatteryLevel getBatteryLevel() const;
    bool        isLowBattery() const;
    bool        isCriticalBattery() const;

    // Deep sleep
    void enterDeepSleep(uint64_t wakeupTimerUs = 0);
    bool wasWokenByTouch() const;
    bool wasWokenByTimer() const;

private:
    uint8_t  _adcPin = 0;
    float    _voltage = 0.0f;          // Smoothed voltage (EMA)
    float    _rawVoltage = 0.0f;       // Latest raw reading
    uint8_t  _percent = 0;             // Reported percentage (with hysteresis)
    uint8_t  _rawPercent = 0;          // Unfiltered percentage from voltage
    uint32_t _lastReadMs = 0;
    bool     _initialized = false;     // First reading flag

    float readRawVoltage();
    uint8_t voltageToPercent(float voltage);
};
