#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

class NetworkManager {
private:
    DNSServer dnsServer;

public:
    void enableAccessPoint();
    bool connectToWiFi(String& ssid, String& password);
};