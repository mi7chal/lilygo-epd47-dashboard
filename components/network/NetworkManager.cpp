#include "NetworkManager.hpp"

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
      retry_timer_(this, retry_timer_callback),
      dns_server_() {}
NetworkManager::~NetworkManager() { deinit(); };


void NetworkManager::init() {
  wifi_.init();
  wifi_.registerEventHandler(NetworkEvent::StationConnectionEstablished, [](void* context, IpAddress ip_address) {
    logger::info("Connected to WiFi network with IP: {}", ip_address.to_string());

    auto* nm = static_cast<NetworkManager*>(context);

    nm->wifi_.enableMDNS("LilyGO Dashboard");  // todo move
  });

  wifi_.registerEventHandler(NetworkEvent::StationDisconnected, [](void* context) {
    logger::debug("Disconnected from WiFi network");
    auto* nm = static_cast<NetworkManager*>(context);

    if (nm->timeout_timer_.isActive()) {
      logger::debug("WiFi connection lost during connection attempt, retrying...");

      nm->retry_timer_.startOnce(1000);  // retry after 1 second
    }
  });

  wifi_.registerEventHandler(NetworkEvent::AccessPointStarted, [](void* context) {
    auto* nm = static_cast<NetworkManager*>(context);

    nm->wifi_.enableMDNS("LilyGO Dashboard AP");  // todo move

    // Start DNS server with the IP of the access point
    if (auto ap_info = nm->wifi_.getAccessPointInfo(); ap_info.has_value()) {
      nm->dns_server_.start(ap_info->ip);
    } else {
      logger::error("Failed to get AP info, starting DNS with fallback IP 192.168.4.1");
      nm->dns_server_.start(IpAddress{192, 168, 4, 1});
    }

    logger::info("Access Point started");
  });

  wifi_.registerEventHandler(NetworkEvent::AccessPointStopped, [](void* context) {
    auto* nm = static_cast<NetworkManager*>(context);
    nm->dns_server_.stop();

    logger::info("Access Point stopped");
  });
}

void NetworkManager::deinit() {
  wifi_.deinit();
  dns_server_.stop();
}

void NetworkManager::enableAccessPoint() {
  logger::debug("Enabling WiFi Captive portal (Access Point) with SSID: {}", kWifiPortalName);

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