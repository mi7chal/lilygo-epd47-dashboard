#include "ConfigManager.h"
#include <ESPmDNS.h>


namespace {

constexpr const char* PREFERENCES_NAMESPACE = "lilygo_dashboard";
constexpr const char* LATITUDE_KEY = "lat";
constexpr const char* LONGITUDE_KEY = "lon";
constexpr const char* API_TOKEN_KEY = "owm_token";

constexpr const char* SSID_KEY = "ssid";
constexpr const char* PASSWORD_KEY = "pass";
}

constexpr uint8_t BOOT_BUTTON_PIN = 0;

ConfigManager::ConfigManager(StorageManager& storage, NetworkManager& network, CaptivePortal& portal) 
    : storage(storage), network(network), portal(portal) {}

void ConfigManager::init() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    Serial.println("[Config] init");

    bool forceConfig = digitalRead(BOOT_BUTTON_PIN) == LOW;

    String ssid = storage.getSSID(), password = storage.getPassword();
    bool wifiConnected = network.connectToWiFi(ssid, password);
    
    if(!wifiConnected) {
        network.enableAccessPoint();
    }

    if(forceConfig || !wifiConnected) {
        configMode = true;
        portal.start(IPAddress(192, 168, 4, 1)); // todo remember to change this address
    }

    Serial.println("[Config] init complete");
}
