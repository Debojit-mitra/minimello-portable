#include "weather_service.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "logger.h"

// =============================================================
// WeatherService — Implementation
// =============================================================

void WeatherService::begin(const String& apiKey, const String& city) {
    _apiKey = apiKey;
    _city = city;

    if (isConfigured()) {
        // Fetch deferred to update() loop so boot isn't blocked
        // fetch();
    }
}

void WeatherService::update(uint32_t nowMs) {
    if (!isConfigured()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    if (_lastFetchMs == 0 || nowMs - _lastFetchMs >= WEATHER_REFRESH_MS) {
        fetch();
    }
}

void WeatherService::fetch() {
    if (!isConfigured()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (_fetching) return;

    _fetching = true;

    String url = String(WEATHER_API_BASE) +
                 "?q=" + _city +
                 "&units=metric" +
                 "&appid=" + _apiKey;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        parseResponse(http.getString());
        _data.lastUpdateMs = millis();
        LOG_I("WEATHER", "Fetched: %.1f°C", _data.tempC);
    } else {
        LOG_E("WEATHER", "Fetch failed (HTTP %d)", code);
    }

    http.end();
    _lastFetchMs = millis();
    _fetching = false;
}

const WeatherData& WeatherService::getData() const {
    return _data;
}

void WeatherService::setCredentials(const String& apiKey, const String& city) {
    _apiKey = apiKey;
    _city = city;
    if (isConfigured()) {
        fetch();
    }
}

bool WeatherService::isConfigured() const {
    return _apiKey.length() > 0 && _city.length() > 0;
}

void WeatherService::parseResponse(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        LOG_E("WEATHER", "JSON parse error: %s", err.c_str());
        return;
    }

    _data.tempC = doc["main"]["temp"].as<float>();
    _data.humidity = doc["main"]["humidity"].as<int>();

    const char* desc = doc["weather"][0]["main"] | "";
    strncpy(_data.description, desc, sizeof(_data.description) - 1);

    const char* name = doc["name"] | "";
    strncpy(_data.cityName, name, sizeof(_data.cityName) - 1);

    int weatherId = doc["weather"][0]["id"].as<int>();
    _data.condition = mapCondition(weatherId);
    _data.valid = true;
}

WeatherCondition WeatherService::mapCondition(int weatherId) {
    // OpenWeatherMap weather condition codes:
    // https://openweathermap.org/weather-conditions
    if (weatherId >= 200 && weatherId < 300) return WeatherCondition::THUNDER;
    if (weatherId >= 300 && weatherId < 400) return WeatherCondition::RAIN;     // Drizzle
    if (weatherId >= 500 && weatherId < 600) return WeatherCondition::RAIN;
    if (weatherId >= 600 && weatherId < 700) return WeatherCondition::SNOW;
    if (weatherId >= 700 && weatherId < 800) return WeatherCondition::MIST;     // Atmosphere
    if (weatherId == 800)                     return WeatherCondition::CLEAR;
    if (weatherId > 800 && weatherId < 900)  return WeatherCondition::CLOUDS;
    return WeatherCondition::UNKNOWN;
}
