#include "NetworkManager.h"

namespace {
    constexpr const char* HOSTNAME = "lilygo-dashboard";
    constexpr const char* WIFI_PORTAL_NAME = "LilyGO Dashboard";
    constexpr const char* WIFI_PORTAL_PASSWORD = "lilygo678";

    constexpr std::uint32_t PORTAL_TIMEOUT_SECONDS = 180U;
    constexpr std::uint32_t WIFI_CONNECTION_TIMEOUT_SECONDS = 30U;
    constexpr byte DNS_PORT = 53;
}

void NetworkManager::enableAccessPoint() {
    Serial.println("[Config] enabling WiFi access point for configuration");
        
        WiFi.mode(WIFI_AP);
        IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(WIFI_PORTAL_NAME, WIFI_PORTAL_PASSWORD); 
        
        dnsServer.start(DNS_PORT, "*", apIP);
    
    Serial.println("[Config] WiFi configuration portal closed");
}

bool NetworkManager::connectToWiFi(String& ssid, String& password) {
    if (ssid.isEmpty()) {
        Serial.println("[Config] no WiFi credentials stored");
        return false;
    }

    Serial.printf("[Config] connecting to WiFi SSID=%s\n", ssid.c_str());


    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(ssid.c_str(), password.c_str());

    const unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_CONNECTION_TIMEOUT_SECONDS * 1000U) {
            Serial.println("[Config] WiFi connection timed out");
            return false;
        }
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    // check connection status one more time after loop
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Config] Failed to connect to WiFi");
        return false;
    }

    Serial.println("[Config] WiFi connected, IP address: " + WiFi.localIP().toString());

     if (MDNS.begin(HOSTNAME)) {
        Serial.println("[Config] MDNS responder started. Acces device under " + String(HOSTNAME) + ".local");
    } else {
        Serial.println("[Config] Error setting up MDNS responder!");
    }

    return true;
}