#include "NetworkManager.hpp"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/inet.h>
#include <mdns.h>
#include <nvs_flash.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include "Timer.hpp"
#include "logger.hpp"


namespace app::network {

namespace {
// todo move these constants to more appropriate place
constexpr const char* kHostname = "lilygo-dashboard";
constexpr const char* kWifiPortalName = "LilyGO Dashboard";
constexpr const char* kWifiPortalPassword = "lilygo678";
constexpr std::uint32_t kWifiConnectionTimeoutSeconds = 30U;

void retry_timer_callback(void* context) {
  auto* nm = static_cast<NetworkManager*>(context);
  nm->connectToWiFi(kWifiPortalName, kWifiPortalPassword);
}

}  // namespace


NetworkManager::NetworkManager()
    : network_state_(),
      wifi_(this, network_state_, kHostname),
      timeout_timer_(this, nullptr),
      retry_timer_(this, retry_timer_callback) {}
NetworkManager::~NetworkManager() { deinit(); };


void NetworkManager::init() {
  wifi_.init();
  wifi_.registerEventHandler(NetworkEvent::StationConnectionEstablished, [](void* context, IpAddress ip_address) {
    logger::info("Connected to WiFi network with IP: {}", ip_address.to_string());

    auto nm = static_cast<NetworkManager*>(context);

    nm->wifi_.enableMDNS("LilyGO Dashboard");
  });

  wifi_.registerEventHandler(NetworkEvent::StationDisconnected, [](void* context) {
    logger::debug("Disconnected from WiFi network");
    auto nm = static_cast<NetworkManager*>(context);

    if (nm->timeout_timer_.isActive()) {
      logger::debug("WiFi connection lost during connection attempt, retrying...");

      nm->retry_timer_.startOnce(1000);  // retry after 1 second
    }
  });

  wifi_.registerEventHandler(NetworkEvent::AccessPointStarted,
                             [](void* context) { logger::info("Access Point started"); });

  wifi_.registerEventHandler(NetworkEvent::AccessPointStopped,
                             [](void* context) { logger::info("Access Point stopped"); });
}

void NetworkManager::deinit() { wifi_.deinit(); }

void NetworkManager::enableAccessPoint() {
  app::logger::debug("Enabling WiFi Captive portal (Access Point) for configuration");

  wifi_.startAccessPoint(kWifiPortalName, kWifiPortalPassword);
}

std::optional<AccessPointInfo> NetworkManager::waitForAccessPointInfo(uint32_t timeout_ms = 30000) const {
  if (!network_state_.waitForApActive(timeout_ms)) {
    return std::nullopt;
  }

  return wifi_.getAccessPointInfo();
}

void NetworkManager::connectToWiFi(const std::string& ssid, const std::string& password) {
  logger::debug("Attempting to connect to WiFi network: {}", ssid);

  timeout_timer_.startOnce(kWifiConnectionTimeoutSeconds * 1000U);

  wifi_.connectToWiFi(ssid, password);
}

std::optional<WiFiConnectionInfo> NetworkManager::waitForWiFiConnectionInfo(uint32_t timeout_ms) const {
  if (!network_state_.waitForStaConnection(timeout_ms)) {
    return std::nullopt;
  }

  return wifi_.getWiFiConnectionInfo();
}

}  // namespace app::network