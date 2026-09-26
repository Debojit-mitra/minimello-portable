#pragma once

#include <Arduino.h>
#include "display_config.h"

// =============================================================
// PowerManager — Battery monitoring, charging detection & deep sleep
// =============================================================
// Reads battery voltage via ADC through 100kΩ/100kΩ divider.
// Detects charging state via TP4056 CHRG pin (active LOW).
// Uses exponential moving average + hysteresis for stable readings.
// Manages deep sleep with GPIO2 (TTP223) wakeup and timer wakeup.


enum class PowerSource : uint8_t {
    BATTERY = 0,         // Running on battery (CHRG HIGH + normal voltage)
    CHARGING,            // USB + battery (CHRG LOW = actively charging)
    USB_POWERED          // USB only, no battery (CHRG HIGH + voltage abnormal)
};

class PowerManager {
public:
    void begin(uint8_t adcPin, uint8_t chrgPin);
    void update(uint32_t nowMs);  // Call periodically

    // Battery readings
    float       getBatteryVoltage() const;
    uint8_t     getBatteryPercent() const;
    bool        isLowBattery() const;
    bool        isCriticalBattery() const;

    // Power source detection
    PowerSource getPowerSource() const;
    bool        isCharging() const;
    bool        isOnUSBPower() const;  // USB only, no battery

    // Deep sleep
    void enterDeepSleep(uint64_t wakeupTimerUs = 0);
    bool wasWokenByTouch() const;
    bool wasWokenByTimer() const;

    // OLED power control — turns off charge pump to save ~15mA in deep sleep
    void displayOff(DisplayType& d);
    void displayOn(DisplayType& d, uint8_t brightness);

    // CPU frequency scaling — drop to 80MHz when WiFi is off
    void setCpuFrequency(uint32_t mhz);

private:
    uint8_t  _adcPin = 0;
    uint8_t  _chrgPin = 0;
    float    _voltage = 0.0f;          // Smoothed voltage (EMA)
    float    _rawVoltage = 0.0f;       // Latest raw reading
    uint8_t  _percent = 0;             // Reported percentage (with hysteresis)
    uint8_t  _rawPercent = 0;          // Unfiltered percentage from voltage
    uint32_t _lastReadMs = 0;
    bool     _initialized = false;     // First reading flag

    // Charging detection state
    PowerSource _powerSource = PowerSource::BATTERY;
    bool     _chrgPinLow = false;      // Raw CHRG pin state (LOW = charging)

    float readRawVoltage();
    uint8_t voltageToPercent(float voltage);
    void detectPowerSource();
};
