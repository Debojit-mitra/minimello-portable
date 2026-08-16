#include "weather/weather_service.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "logger.h"

// =============================================================
// WeatherService — Implementation
// =============================================================

void WeatherService::begin(float lat, float lon, const String& city) {
    _lat = lat;
    _lon = lon;
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
                 "?latitude=" + String(_lat, 4) +
                 "&longitude=" + String(_lon, 4) +
                 "&current=temperature_2m,relative_humidity_2m,weather_code";

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        parseResponse(http.getString());
        _data.lastUpdateMs = millis();
        LOG_I("WEATHER", "Fetched: %.1f°C", _data.tempC);
        _lastFetchMs = millis();
    } else {
        LOG_E("WEATHER", "Fetch failed (HTTP %d). Retrying in 60s.", code);
        // If it fails, fake the last fetch time so it retries in 60 seconds 
        // instead of waiting the full 30 minutes.
        _lastFetchMs = millis() - WEATHER_REFRESH_MS + 60000;
    }

    http.end();
    _fetching = false;
}

const WeatherData& WeatherService::getData() const {
    return _data;
}

void WeatherService::setCredentials(float lat, float lon, const String& city) {
    _lat = lat;
    _lon = lon;
    _city = city;
    if (isConfigured()) {
        fetch();
    }
}

bool WeatherService::isConfigured() const {
    // Basic check to see if lat/lon are not precisely zero and city is set
    return (_lat != 0.0f || _lon != 0.0f);
}

void WeatherService::parseResponse(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        LOG_E("WEATHER", "JSON parse error: %s", err.c_str());
        return;
    }

    _data.tempC = doc["current"]["temperature_2m"].as<float>();
    _data.humidity = doc["current"]["relative_humidity_2m"].as<int>();

    int weatherCode = doc["current"]["weather_code"].as<int>();
    
    const char* descStr = "Unknown";
    if (weatherCode == 0) descStr = "Clear sky";
    else if (weatherCode >= 1 && weatherCode <= 3) descStr = "Cloudy";
    else if (weatherCode >= 45 && weatherCode <= 48) descStr = "Foggy";
    else if (weatherCode >= 51 && weatherCode <= 57) descStr = "Drizzle";
    else if (weatherCode >= 61 && weatherCode <= 67) descStr = "Rain";
    else if (weatherCode >= 71 && weatherCode <= 77) descStr = "Snow";
    else if (weatherCode >= 80 && weatherCode <= 82) descStr = "Showers";
    else if (weatherCode >= 85 && weatherCode <= 86) descStr = "Snow Showers";
    else if (weatherCode >= 95 && weatherCode <= 99) descStr = "Thunderstorm";

    strncpy(_data.description, descStr, sizeof(_data.description) - 1);

    // Copy city name over, truncating at the first comma to ensure it fits on the OLED
    int commaIdx = _city.indexOf(',');
    String shortCity = (commaIdx != -1) ? _city.substring(0, commaIdx) : _city;
    strncpy(_data.cityName, shortCity.c_str(), sizeof(_data.cityName) - 1);
    _data.cityName[sizeof(_data.cityName) - 1] = '\0';

    _data.condition = mapCondition(weatherCode);
    _data.valid = true;
}

WeatherCondition WeatherService::mapCondition(int weatherId) {
    // Open-Meteo uses WMO Weather interpretation codes
    // https://open-meteo.com/en/docs
    
    if (weatherId == 0) return WeatherCondition::CLEAR;
    if (weatherId >= 1 && weatherId <= 3) return WeatherCondition::CLOUDS;
    if (weatherId >= 45 && weatherId <= 48) return WeatherCondition::MIST; // Fog
    if (weatherId >= 51 && weatherId <= 67) return WeatherCondition::RAIN; // Drizzle & Rain
    if (weatherId >= 71 && weatherId <= 77) return WeatherCondition::SNOW; // Snow
    if (weatherId >= 80 && weatherId <= 82) return WeatherCondition::RAIN; // Showers
    if (weatherId >= 85 && weatherId <= 86) return WeatherCondition::SNOW; // Snow showers
    if (weatherId >= 95 && weatherId <= 99) return WeatherCondition::THUNDER; // Thunderstorm
    
    return WeatherCondition::UNKNOWN;
}
