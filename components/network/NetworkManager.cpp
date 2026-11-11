#include "NetworkManager.hpp"

#include <algorithm>

#include "logger.hpp"


namespace app::network {

namespace {
// todo move these constants to more appropriate place
constexpr const char* kHostname = "lilygo-dashboard";
constexpr const char* kWifiPortalName = "LilyGO Dashboard";
constexpr const char* kWifiPortalPassword = "lilygo678";
constexpr std::uint32_t kWifiConnectionTimeoutSeconds = 30U;
constexpr std::uint32_t kRetryBaseDelayMs = 1000U;
constexpr std::uint32_t kRetryMaxDelayMs = 30000U;
constexpr std::uint32_t kRetryMaxShift = 5U;  // 1s, 2s, 4s ... capped by kRetryMaxDelayMs

constexpr std::uint32_t retryDelayMs(std::uint32_t attempt) {
  return std::min(kRetryBaseDelayMs << std::min(attempt, kRetryMaxShift), kRetryMaxDelayMs);
}


}  // namespace


void NetworkManager::retry_timer_callback(void* context) {
  auto* nm = static_cast<NetworkManager*>(context);
  // credentials stay in the WiFi driver, only connect again
  if (nm->want_connected_ && !nm->network_state_.isStaConnected()) {
    nm->wifi_.reconnect();
  }
}

// battery matters: give up instead of keeping the radio on forever, so the device can go back to deep sleep
void NetworkManager::timeout_timer_callback(void* context) {
  auto* nm = static_cast<NetworkManager*>(context);
  logger::warn("WiFi connection timed out, giving up");

  nm->want_connected_ = false;
  nm->retry_timer_.stop();
  nm->wifi_.disconnectFromNetwork();
}

NetworkManager::NetworkManager()
    : network_state_(),
      wifi_(this, network_state_, kHostname),
      dns_server_(),
      timeout_timer_(this, timeout_timer_callback),
      retry_timer_(this, retry_timer_callback) {}
NetworkManager::~NetworkManager() { deinit(); };


bool NetworkManager::init() {
  if (!wifi_.init()) {
    return false;
  }

  wifi_.registerEventHandler(NetworkEvent::StationConnectionEstablished, [](void* context, IpAddress ip_address) {
    logger::info("Connected to WiFi network with IP: {}", ip_address.to_string());

    auto* nm = static_cast<NetworkManager*>(context);
    nm->timeout_timer_.stop();
    nm->retry_timer_.stop();
    nm->retry_attempt_ = 0;

    nm->wifi_.enableMDNS("LilyGO Dashboard");  // todo move
  });

  wifi_.registerEventHandler(NetworkEvent::StationDisconnected, [](void* context) {
    logger::debug("Disconnected from WiFi network");
    auto* nm = static_cast<NetworkManager*>(context);

    if (!nm->want_connected_) {
      return;
    }

    // a link lost after connecting gets the same bounded retry window
    if (!nm->timeout_timer_.isActive()) {
      nm->timeout_timer_.startOnce(kWifiConnectionTimeoutSeconds * 1000U);
    }

    logger::debug("WiFi connection lost, retrying...");
    nm->retry_timer_.startOnce(retryDelayMs(nm->retry_attempt_.fetch_add(1)));
  });

  wifi_.registerEventHandler(NetworkEvent::AccessPointStarted, [](void* context) {
    auto* nm = static_cast<NetworkManager*>(context);

    nm->wifi_.enableMDNS("LilyGO Dashboard AP");  // todo move

    // Start DNS server with the IP of the access point
    if (auto ap_info = nm->wifi_.getAccessPointInfo(); ap_info.has_value()) {
      nm->dns_server_.start(ap_info->ip);
    } else {
      logger::warn("Failed to get AP info, starting DNS with fallback IP 192.168.4.1");
      nm->dns_server_.start(IpAddress{192, 168, 4, 1});
    }

    logger::info("Access Point started");
  });

  wifi_.registerEventHandler(NetworkEvent::AccessPointStopped, [](void* context) {
    auto* nm = static_cast<NetworkManager*>(context);
    nm->dns_server_.stop();

    logger::info("Access Point stopped");
  });

  logger::info("NetworkManager initialized");
  return true;
}

void NetworkManager::deinit() {
  want_connected_ = false;
  timeout_timer_.stop();
  retry_timer_.stop();

  wifi_.deinit();
  dns_server_.stop();
}

void NetworkManager::enableAccessPoint() {
  logger::debug("Enabling WiFi Captive portal (Access Point) with SSID: {}", kWifiPortalName);

  wifi_.startAccessPoint(kWifiPortalName, kWifiPortalPassword);
}

std::optional<AccessPointInfo> NetworkManager::waitForAccessPointInfo(uint32_t timeout_ms) const {
  if (!network_state_.waitForApActive(timeout_ms)) {
    return std::nullopt;
  }

  return wifi_.getAccessPointInfo();
}

void NetworkManager::connectToWiFi(const std::string& ssid, const std::string& password) {
  logger::debug("Attempting to connect to WiFi network: {}", ssid);

  want_connected_ = true;
  retry_attempt_ = 0;
  retry_timer_.stop();
  timeout_timer_.startOnce(kWifiConnectionTimeoutSeconds * 1000U);

  wifi_.connectToNetwork(ssid, password);
}

std::optional<WiFiConnectionInfo> NetworkManager::waitForWiFiConnectionInfo(uint32_t timeout_ms) const {
  if (!network_state_.waitForStaConnection(timeout_ms)) {
    return std::nullopt;
  }

  return wifi_.getWiFiConnectionInfo();
}

}  // namespace app::network