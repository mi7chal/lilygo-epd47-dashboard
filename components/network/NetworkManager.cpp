#include "NetworkManager.hpp"

#include <algorithm>

#include "logger.hpp"


namespace app::network {

namespace {
// todo move these constants to more appropriate place
constexpr const char* kHostname = "lilygo-dashboard";
constexpr const char* kWifiPortalName = "LilyGO Dashboard";
constexpr const char* kWifiPortalPassword = "lilygo678";
constexpr std::uint32_t kRetryBaseDelayMs = 1000U;
constexpr std::uint32_t kRetryMaxDelayMs = 30000U;
constexpr std::uint32_t kRetryMaxShift = 5U;  // 1s, 2s, 4s ... capped by kRetryMaxDelayMs

ESP_EVENT_DEFINE_BASE(NETWORK_TIMER_EVENT);

enum class TimerEventId : std::int32_t {
  Timeout = 1,
  Retry = 2,
};

constexpr std::uint32_t retryDelayMs(std::uint32_t attempt) {
  return std::min(kRetryBaseDelayMs << std::min(attempt, kRetryMaxShift), kRetryMaxDelayMs);
}

constexpr bool isTerminalDisconnect(std::uint8_t reason, std::uint32_t retry_attempt) {
  switch (reason) {
    case WIFI_REASON_802_1X_AUTH_FAILED:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
      return true;
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return retry_attempt >= 1;
    default:
      return false;
  }
}

}  // namespace

void NetworkManager::retry_timer_callback(void* context) {
  auto* nm = static_cast<NetworkManager*>(context);
  const std::uint32_t session = nm->sta_session_id_.load(std::memory_order_acquire);
  esp_event_post(NETWORK_TIMER_EVENT, static_cast<std::int32_t>(TimerEventId::Retry), &session, sizeof(session), 0);
}

// battery matters: give up instead of keeping the radio on forever, so the device can go back to deep sleep
void NetworkManager::timeout_timer_callback(void* context) {
  auto* nm = static_cast<NetworkManager*>(context);
  const std::uint32_t session = nm->sta_session_id_.load(std::memory_order_acquire);
  esp_event_post(NETWORK_TIMER_EVENT, static_cast<std::int32_t>(TimerEventId::Timeout), &session, sizeof(session), 0);
}

void NetworkManager::timer_event_handler(void* arg, const char* /*base*/, std::int32_t id, void* data) {
  if (arg == nullptr || data == nullptr) {
    return;
  }
  auto* nm = static_cast<NetworkManager*>(arg);
  const auto session_id = *static_cast<const std::uint32_t*>(data);
  nm->handleTimerEvent(id, session_id);
}

void NetworkManager::handleTimerEvent(std::int32_t id, std::uint32_t session_id) {
  if (session_id != sta_session_id_.load(std::memory_order_acquire) || !want_connected_) {
    return;
  }

  switch (static_cast<TimerEventId>(id)) {
    case TimerEventId::Retry:
      // credentials stay in the WiFi driver, only connect again
      if (!network_state_.isStaConnected() && !wifi_.reconnect()) {
        cancelStationAttempt();
        wifi_.disconnectFromNetwork();
      }
      break;
    case TimerEventId::Timeout:
      logger::warn("WiFi connection timed out, giving up");
      cancelStationAttempt();
      wifi_.disconnectFromNetwork();
      break;
    default:
      break;
  }
}

void NetworkManager::cancelStationAttempt() {
  want_connected_ = false;
  sta_session_id_.fetch_add(1, std::memory_order_acq_rel);
  retry_attempt_ = 0;
  timeout_timer_.stop();
  retry_timer_.stop();
}

NetworkManager::NetworkManager()
    : network_state_(),
      wifi_(this, network_state_, kHostname),
      dns_server_(),
      timeout_timer_(this, timeout_timer_callback),
      retry_timer_(this, retry_timer_callback) {}
NetworkManager::~NetworkManager() { deinit(); };

bool NetworkManager::init() {
  if (initialized_) {
    return true;
  }

  if (!wifi_.init()) {
    return false;
  }

  const esp_err_t err = esp_event_handler_instance_register(
      NETWORK_TIMER_EVENT, ESP_EVENT_ANY_ID, &NetworkManager::timer_event_handler, this, &timer_event_handler_);
  if (err != ESP_OK) {
    logger::error("Failed to register network timer event handler: {}", esp_err_to_name(err));
    wifi_.deinit();
    return false;
  }

  wifi_.registerEventHandler(NetworkEvent::StationConnectionEstablished, [](void* context, IpAddress ip_address) {
    logger::info("Connected to WiFi network with IP: {}", ip_address);

    auto* nm = static_cast<NetworkManager*>(context);
    nm->timeout_timer_.stop();
    nm->retry_timer_.stop();
    nm->retry_attempt_ = 0;

    nm->wifi_.enableMDNS("LilyGO Dashboard");  // todo move
  });

  wifi_.registerEventHandler(NetworkEvent::StationDisconnected, [](void* context, std::uint8_t reason, bool terminal) {
    logger::debug("Disconnected from WiFi network (reason: {})", reason);
    auto* nm = static_cast<NetworkManager*>(context);

    if (!nm->want_connected_) {
      return;
    }

    const std::uint32_t attempt = nm->retry_attempt_.load(std::memory_order_acquire);
    if (terminal || isTerminalDisconnect(reason, attempt)) {
      logger::warn("WiFi connection failed permanently (reason: {}), giving up", reason);
      nm->cancelStationAttempt();
      nm->wifi_.disconnectFromNetwork();
      return;
    }

    // a link lost after connecting gets the same bounded retry window
    if (!nm->timeout_timer_.isActive()) {
      nm->timeout_timer_.startOnce(kConnectionTimeoutMs);
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
    nm->dns_server_.requestStop();

    logger::info("Access Point stopped");
  });

  initialized_ = true;
  logger::info("NetworkManager initialized");
  return true;
}

void NetworkManager::deinit() {
  cancelStationAttempt();
  dns_server_.requestStop();

  wifi_.deinit();

  if (timer_event_handler_ != nullptr) {
    esp_event_handler_instance_unregister(NETWORK_TIMER_EVENT, ESP_EVENT_ANY_ID, timer_event_handler_);
    timer_event_handler_ = nullptr;
  }

  dns_server_.stop();
  initialized_ = false;
}

void NetworkManager::enableAccessPoint() {
  cancelStationAttempt();
  if (!initialized_) {
    logger::warn("Attempted to enable Access Point without initialization");
    return;
  }

  logger::debug("Enabling WiFi Captive portal (Access Point) with SSID: {}", kWifiPortalName);
  if (!wifi_.startAccessPoint(kWifiPortalName, kWifiPortalPassword)) {
    logger::error("Failed to enable Access Point");
  }
}

std::optional<AccessPointInfo> NetworkManager::waitForAccessPointInfo(uint32_t timeout_ms) const {
  if (!network_state_.waitForApActive(timeout_ms)) {
    return std::nullopt;
  }

  return wifi_.getAccessPointInfo();
}

void NetworkManager::connectToWiFi(const std::string& ssid, const std::string& password) {
  cancelStationAttempt();
  if (!initialized_) {
    logger::warn("Attempted to connect to WiFi without initialization");
    wifi_.disconnectFromNetwork();
    return;
  }
  if (!WiFiAdapter::isValidCredentials(ssid, password)) {
    (void)wifi_.connectToNetwork(ssid, password);  // logs warning and marks STA failed without arming timer
    return;
  }

  logger::debug("Attempting to connect to WiFi network: {}", ssid);

  want_connected_ = true;
  timeout_timer_.startOnce(kConnectionTimeoutMs);

  if (!wifi_.connectToNetwork(ssid, password)) {
    cancelStationAttempt();
  }
}

std::optional<WiFiConnectionInfo> NetworkManager::waitForWiFiConnectionInfo(uint32_t timeout_ms) const {
  if (!network_state_.waitForStaConnection(timeout_ms)) {
    return std::nullopt;
  }

  return wifi_.getWiFiConnectionInfo();
}
}  // namespace app::network
