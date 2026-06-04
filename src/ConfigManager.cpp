#include "ConfigManager.h"
#include <driver/gpio.h>
#include <esp_err.h>
#include "Log.h"
#include "NetworkManager.h"
#include "StorageManager.h"
#include "WebPortal.h"

namespace {
constexpr gpio_num_t CONFIG_BUTTON_PIN = GPIO_NUM_21;

bool configureButtonInput() {
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << CONFIG_BUTTON_PIN;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    const esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        logger::warn("failed to configure config button GPIO {}: {}", static_cast<int>(CONFIG_BUTTON_PIN), esp_err_to_name(err));
        return false;
    }

    return true;
}
}

ConfigManager::ConfigManager(StorageManager& storage, NetworkManager& network, WebPortal& portal) 
    : storage(storage), network(network), portal(portal) {}

std::variant<STAConfigState, APConfigState, NormalState> ConfigManager::init() {
    logger::info("init");


    // esp-idf init functions (which must be called exactly once!)
    const esp_err_t netifErr = esp_netif_init();
    if (netifErr != ESP_OK && netifErr != ESP_ERR_INVALID_STATE) {
        logger::error("esp_netif_init failed: {}", esp_err_to_name(netifErr));
        return false;
    }

    const esp_err_t eventLoopErr = esp_event_loop_create_default();
    if (eventLoopErr != ESP_OK && eventLoopErr != ESP_ERR_INVALID_STATE) {
        logger::error("esp_event_loop_create_default failed: {}", esp_err_to_name(eventLoopErr));
        return false;
    }

    const bool buttonConfigured = configureButtonInput();
    const bool forceConfig = buttonConfigured && gpio_get_level(CONFIG_BUTTON_PIN) == 0;

    logger::info("config button state: {} (hold button during boot to force config mode)", forceConfig ? "PRESSED" : "NOT PRESSED");

    std::string ssid = storage.getSSID();
    std::string password = storage.getPassword();
    
    logger::info("loaded credentials from storage: ssid=\"{}\", pass=\"***\"", ssid.c_str());

    auto wifiConnection = network.connectToWiFi(ssid, password);

    std::variant<STAConfigState, APConfigState, NormalState> result;
    
    if(!wifiConnection.has_value()) {
        auto accessPointInfo = network.enableAccessPoint();
        configMode = true;
        portal.start();

        result = APConfigState{
            .ssid = accessPointInfo.ssid,
            .password = accessPointInfo.password,
            .hostname = accessPointInfo.hostname,
            .ip = accessPointInfo.ip
        };
    } else if(forceConfig) {
        configMode = true;
        portal.start(); 

        result = STAConfigState{
            .ssid = wifiConnection->ssid,
            .hostname = wifiConnection->hostname,
            .ip = wifiConnection->ip,
            .rssi = wifiConnection->rssi
        };
    } else {
        // If we have pending geocode (saved while offline), try resolving now.
        if (storage.isGeocodePending()) {
            logger::info("Geocode pending — attempting now");
            if (portal.attemptGeocodeNow()) {
                logger::info("Geocode succeeded on boot");
            } else {
                logger::warn("Geocode failed on boot, will remain pending");
            }
        }

        result = NormalState{
            wifiConnection->ssid,
            wifiConnection->ip,
            wifiConnection->rssi
        };
    }

    logger::info("init complete");
    return result;
}
