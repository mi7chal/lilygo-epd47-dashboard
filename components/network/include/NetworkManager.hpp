#pragma once

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

  void init();
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
};

}  // namespace app::network