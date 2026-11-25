#pragma once

#include <esp_event.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "DnsServer.hpp"
#include "NetworkState.hpp"
#include "Timer.hpp"
#include "WiFiAdapter.hpp"

namespace app::network {

// todo consider if state shouldn't be stored somewhere else

class NetworkManager {
 public:
  static constexpr std::uint32_t kConnectionTimeoutMs = 30000U;

  NetworkManager();
  ~NetworkManager();
  NetworkManager(const NetworkManager&) = delete;
  NetworkManager& operator=(const NetworkManager&) = delete;
  NetworkManager(NetworkManager&&) = delete;
  NetworkManager& operator=(NetworkManager&&) = delete;

  bool init();
  void deinit();

  void enableAccessPoint();
  void connectToWiFi(const std::string& ssid, const std::string& password);

  std::optional<AccessPointInfo> waitForAccessPointInfo(uint32_t timeout_ms = kConnectionTimeoutMs) const;
  std::optional<WiFiConnectionInfo> waitForWiFiConnectionInfo(uint32_t timeout_ms = kConnectionTimeoutMs + 1000U) const;

  [[nodiscard]] std::optional<AccessPointInfo> getAccessPointInfo() const { return wifi_.getAccessPointInfo(); }
  [[nodiscard]] std::optional<WiFiConnectionInfo> getWiFiConnectionInfo() const {
    return wifi_.getWiFiConnectionInfo();
  }
  [[nodiscard]] const NetworkState& getNetworkState() const { return network_state_; }

 private:
  void cancelStationAttempt();
  void handleTimerEvent(std::int32_t id, std::uint32_t session_id);

  NetworkState network_state_;
  WiFiAdapter wifi_;
  DnsServer dns_server_;

  utils::Timer timeout_timer_;
  utils::Timer retry_timer_;

  bool initialized_{false};
  esp_event_handler_instance_t timer_event_handler_{nullptr};

  // lost links are retried only while true
  std::atomic<bool> want_connected_{false};
  std::atomic<std::uint32_t> retry_attempt_{0};
  std::atomic<std::uint32_t> sta_session_id_{0};

  static void retry_timer_callback(void* context);
  static void timeout_timer_callback(void* context);
  static void timer_event_handler(void* arg, const char* base, std::int32_t id, void* data);
};
}  // namespace app::network
