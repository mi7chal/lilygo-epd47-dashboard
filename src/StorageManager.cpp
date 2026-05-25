#include "StorageManager.h"

namespace {
    constexpr const char* PREFERENCES_NAMESPACE = "lilygo_dashboard";

    constexpr const char* LATITUDE_KEY = "lat";
    constexpr const char* LONGITUDE_KEY = "lon";
    constexpr const char* API_TOKEN_KEY = "owm_token";

    constexpr const char* SSID_KEY = "ssid";
    constexpr const char* PASSWORD_KEY = "pass";
}

void StorageManager::cacheInit() {
    if(cacheInitialised) {
        return;
    }

    preferences.begin(PREFERENCES_NAMESPACE, false);
    
    ssid = preferences.getString(SSID_KEY, "");
    password = preferences.getString(PASSWORD_KEY, "");
    apiToken = preferences.getString(API_TOKEN_KEY, "");
    
    latitude = preferences.getString(LATITUDE_KEY, "52.2297"); 
    longitude = preferences.getString(LONGITUDE_KEY, "21.0122"); // default to Warsaw city center coordinates
    
    preferences.end();

    cacheInitialised = true;
    
    Serial.println("[Storage] Configuration loaded to RAM.");
}

void StorageManager::saveStringIfChanged(const char* key, String& currentValue, const String& newValue) {
    if(!preferences.arePreferencesOpened()) {
        return;
    }

    if (currentValue != newValue) {
        preferences.putString(key, newValue);
        currentValue = newValue; 
        Serial.printf("[Storage] Updated NVS key: %s\n", key);
    }
}

void StorageManager::saveNetworkCredentials(const String& newSsid, const String& newPass) {
    preferences.begin(PREFERENCES_NAMESPACE, false);

    saveStringIfChanged(SSID_KEY, ssid, newSsid);
    saveStringIfChanged(PASSWORD_KEY, password, newPass);
    
    preferences.end();
    Serial.println("[Storage] Saved network credentials.");
}

void StorageManager::saveAppSettings(const String& newToken, const String& newLat, const String& newLon) {
    preferences.begin(PREFERENCES_NAMESPACE, false);

    saveStringIfChanged(API_TOKEN_KEY, apiToken, newToken);
    saveStringIfChanged(LATITUDE_KEY, latitude, newLat);
    saveStringIfChanged(LONGITUDE_KEY, longitude, newLon);
    
    preferences.end();
    Serial.println("[Storage] Saved application settings.");
}