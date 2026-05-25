#pragma once
#include <Arduino.h>
#include "PreferencesProxy.h"

class StorageManager {
private:
    bool cacheInitialised = false;

    PreferencesProxy preferences;

    String ssid;
    String password;
    String apiToken;
    String latitude;
    String longitude;

    void saveStringIfChanged(const char* key, String& currentValue, const String& newValue);

public:
    StorageManager() = default;
    
    void cacheInit(); 
    
    void saveNetworkCredentials(const String& newSsid, const String& newPass);
    void saveAppSettings(const String& newToken, const String& newLat, const String& newLon);
    
    [[nodiscard]] const String& getSSID() const { return ssid; }
    [[nodiscard]] const String& getPassword() const { return password; }
    [[nodiscard]] const String& getApiToken() const { return apiToken; }
    [[nodiscard]] const String& getLatitude() const { return latitude; }
    [[nodiscard]] const String& getLongitude() const { return longitude; }
};