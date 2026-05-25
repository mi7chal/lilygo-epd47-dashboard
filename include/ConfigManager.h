#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <cstdint>

#include "StorageManager.h"
#include "NetworkManager.h"
#include "CaptivePortal.h"

class ConfigManager {
private:
    StorageManager& storage;
    NetworkManager& network;
    CaptivePortal& portal;

    bool configMode = false;
public:
    ConfigManager(StorageManager& storage, NetworkManager& network, CaptivePortal& portal);


    void init();

    [[nodiscard]] bool isConfigMode() const noexcept { return configMode;}
};