#pragma once

#include <cstdint>
#include <string>
#include <variant>

class StorageManager;
class NetworkManager;
class WebPortal;

struct APConfigState {
    std::string ssid;
    std::string password;
    std::string hostname;
    std::string ip;
};

struct STAConfigState {
    std::string ssid;
    std::string hostname;
    std::string ip;
    int32_t rssi; 
};

struct NormalState {
    std::string ssid;
    std::string ip;
    int32_t rssi; 
};

class ConfigManager {
private:
    StorageManager& storage;
    NetworkManager& network;
    WebPortal& portal;

    bool configMode = false;
public:
    ConfigManager(StorageManager& storage, NetworkManager& network, WebPortal& portal);

    std::variant<STAConfigState, APConfigState, NormalState> init();

    [[nodiscard]] bool isConfigMode() const noexcept { return configMode;}
};