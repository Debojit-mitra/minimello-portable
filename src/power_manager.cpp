#include "power_manager.h"
#include "config.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

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
    { 4150,  95 },
    { 4100,  90 },
    { 4050,  85 },
    { 4000,  80 },
    { 3950,  75 },
    { 3900,  70 },
    { 3850,  65 },
    { 3800,  55 },
    { 3750,  45 },
    { 3700,  38 },
    { 3650,  30 },
    { 3600,  22 },
    { 3550,  17 },
    { 3500,  12 },
    { 3450,   8 },
    { 3400,   5 },
    { 3350,   2 },
    { 3300,   0 },
};
static const int CURVE_SIZE = sizeof(DISCHARGE_CURVE) / sizeof(DISCHARGE_CURVE[0]);

void PowerManager::begin(uint8_t adcPin) {
    _adcPin = adcPin;
#if ENABLE_BATTERY_MODULE
    analogSetAttenuation(ADC_11db);  // Full range (~0-2.6V input)
    pinMode(_adcPin, INPUT);

    // Take initial reading immediately
    _rawVoltage = readRawVoltage();
    _voltage = _rawVoltage;
    _rawPercent = voltageToPercent(_voltage);
    _percent = _rawPercent;
#else
    _rawVoltage = 4.2f;
    _voltage = 4.2f;
    _rawPercent = 100;
    _percent = 100;
#endif
    
    _initialized = true;
    _lastReadMs = millis();
}

void PowerManager::update(uint32_t nowMs) {
#if ENABLE_BATTERY_MODULE
    if (nowMs - _lastReadMs >= BATTERY_READ_INTERVAL) {
        _lastReadMs = nowMs;
        
        _rawVoltage = readRawVoltage();
        
        // Exponential Moving Average (EMA) to smooth out sudden voltage sags from WiFi
        _voltage = (_voltage * 0.85f) + (_rawVoltage * 0.15f);
        
        _rawPercent = voltageToPercent(_voltage);
        
        // Hysteresis: Only update the user-facing percentage if it changes by at least 2%
        // This prevents the number from constantly toggling (e.g., 66% -> 67% -> 66%)
        if (abs((int)_rawPercent - (int)_percent) >= 2) {
            _percent = _rawPercent;
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

BatteryLevel PowerManager::getBatteryLevel() const {
    if (_percent >= 80) return BatteryLevel::BATT_FULL;
    if (_percent >= 50) return BatteryLevel::BATT_GOOD;
    if (_percent >= 25) return BatteryLevel::BATT_MEDIUM;
    if (_percent >= BATTERY_LOW_PERCENT) return BatteryLevel::BATT_LOW;
    return BatteryLevel::BATT_CRITICAL;
}

bool PowerManager::isLowBattery() const {
    return _percent <= BATTERY_LOW_PERCENT;
}

bool PowerManager::isCriticalBattery() const {
    return _percent <= BATTERY_CRITICAL_PERCENT;
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
    // Average multiple ADC samples for stability
    uint32_t sum = 0;
    const int NUM_SAMPLES = 32;  // Increased for better noise reduction
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += analogReadMilliVolts(_adcPin);
        delayMicroseconds(500);  // Spread samples out slightly
    }
    float adcMv = (float)sum / NUM_SAMPLES;

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
