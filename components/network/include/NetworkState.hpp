#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace app::network {

// forward declaration to avoid circular dependency
class WiFiAdapter;

class NetworkState {
 public:
  NetworkState() { group_ = xEventGroupCreate(); }

  ~NetworkState() {
    if (group_ != nullptr) {
      vEventGroupDelete(group_);
      group_ = nullptr;
    }
  }

  bool isConnected() const {
    EventBits_t bits = xEventGroupGetBits(group_);
    return (bits & CONNECTED_BIT) != 0;
  }

  bool hasFailed() const {
    EventBits_t bits = xEventGroupGetBits(group_);
    return (bits & FAIL_BIT) != 0;
  }

  bool waitForConnection(uint32_t timeout_ms) {
    EventBits_t bits =
        xEventGroupWaitBits(group_, CONNECTED_BIT | FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & CONNECTED_BIT) != 0;
  }

 private:
  EventGroupHandle_t group_;

  static constexpr int CONNECTED_BIT = BIT0;
  static constexpr int FAIL_BIT = BIT1;

  friend class WiFiAdapter;

  void setConnected() {
    xEventGroupClearBits(group_, FAIL_BIT);
    xEventGroupSetBits(group_, CONNECTED_BIT);
  }

  void setDisconnected() {
    xEventGroupClearBits(group_, CONNECTED_BIT);
    xEventGroupSetBits(group_, FAIL_BIT);
  }
};

}  // namespace app::network