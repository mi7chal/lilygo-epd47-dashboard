#pragma once

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

// todo add dhcp 114 and dns

class NetworkManager {
 public:
  NetworkManager();
  ~NetworkManager();

  bool init();
  void deinit();

  void enableAccessPoint();
  void connectToWiFi(const std::string& ssid, const std::string& password);

  std::optional<AccessPointInfo> waitForAccessPointInfo(
      uint32_t timeout_ms = 30000) const;  // todo change timeout to some constant or sth
  std::optional<WiFiConnectionInfo> waitForWiFiConnectionInfo(uint32_t timeout_ms = 30000) const;

  [[nodiscard]] std::optional<AccessPointInfo> getAccessPointInfo() const { return wifi_.getAccessPointInfo(); }
  [[nodiscard]] std::optional<WiFiConnectionInfo> getWiFiConnectionInfo() const {
    return wifi_.getWiFiConnectionInfo();
  }
  [[nodiscard]] const NetworkState& getNetworkState() const { return network_state_; }

 private:
  NetworkState network_state_;
  WiFiAdapter wifi_;
  DnsServer dns_server_;

  utils::Timer timeout_timer_;
  utils::Timer retry_timer_;

  // lost links are retried only while true
  std::atomic<bool> want_connected_{false};
  std::atomic<std::uint32_t> retry_attempt_{0};

  static void retry_timer_callback(void* context);
  static void timeout_timer_callback(void* context);
};

}  // namespace app::network