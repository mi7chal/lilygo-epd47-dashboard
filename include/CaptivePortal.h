#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "StorageManager.h"

class CaptivePortal {
public:
    CaptivePortal(StorageManager& storage);

    void start(IPAddress apIP);
    void processDNS();

private:
    StorageManager& storage;
    AsyncWebServer server;
    DNSServer dnsServer;
    void setupEndpoints();

};
