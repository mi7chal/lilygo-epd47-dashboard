#pragma once

#include <optional>
#include <string>

#include "NetworkState.hpp"

namespace app::network {

struct WiFiConnectionInfo {
  std::string ssid;
  std::string hostname;
  std::string ip;
  int32_t rssi;
};

struct AccessPointInfo {
  std::string ssid;
  std::string password;
  std::string hostname;
  std::string ip;
};

class NetworkManager {
 public:
  AccessPointInfo enableAccessPoint();
  std::optional<WiFiConnectionInfo> connectToWiFi(const std::string& ssid, const std::string& password);


  [[nodiscard]] const NetworkState& getNetworkState() const { return network_state_; }

 private:
  NetworkState network_state_;
};

}  // namespace app::network