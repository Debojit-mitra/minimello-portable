#pragma once

#include <Arduino.h>
#include "clock_engine.h"  // for WeatherData, WeatherCondition

// =============================================================
// WeatherService — OpenWeatherMap client
// =============================================================

class WeatherService {
public:
    void begin(const String& apiKey, const String& city);
    void update(uint32_t nowMs);

    // Force a fetch
    void fetch();

    // Get cached data
    const WeatherData& getData() const;

    // Update credentials (from WebUI)
    void setCredentials(const String& apiKey, const String& city);

    bool isConfigured() const;

private:
    String      _apiKey;
    String      _city;
    WeatherData _data;
    uint32_t    _lastFetchMs = 0;
    bool        _fetching = false;

    void parseResponse(const String& json);
    WeatherCondition mapCondition(int weatherId);
};
