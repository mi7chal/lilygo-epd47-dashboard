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

  [[nodiscard]] bool isConnected() const {
    EventBits_t bits = xEventGroupGetBits(group_);
    return (bits & (kStaConnectedBit | kApConnectedBit)) != 0;
  }

  [[nodiscard]] bool isStaConnected() const { return (xEventGroupGetBits(group_) & kStaConnectedBit) != 0; }
  [[nodiscard]] bool hasStaFailed() const { return (xEventGroupGetBits(group_) & kStaFailBit) != 0; }

  [[nodiscard]] bool isApActive() const { return (xEventGroupGetBits(group_) & kApConnectedBit) != 0; }
  [[nodiscard]] bool hasApFailed() const { return (xEventGroupGetBits(group_) & kApFailBit) != 0; }


  bool waitForAnyConnection(uint32_t timeout_ms) const {
    EventBits_t bits = xEventGroupWaitBits(group_, kStaConnectedBit | kApConnectedBit | kStaFailBit | kApFailBit,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & (kStaConnectedBit | kApConnectedBit)) != 0;
  }

  bool waitForStaConnection(uint32_t timeout_ms) const {
    EventBits_t bits =
        xEventGroupWaitBits(group_, kStaConnectedBit | kStaFailBit, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & kStaConnectedBit) != 0;
  }

  bool waitForApActive(uint32_t timeout_ms) const {
    EventBits_t bits =
        xEventGroupWaitBits(group_, kApConnectedBit | kApFailBit, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & kApConnectedBit) != 0;
  }

 private:
  EventGroupHandle_t group_;

  static constexpr int kStaConnectedBit = BIT0;
  static constexpr int kStaFailBit = BIT1;
  static constexpr int kApConnectedBit = BIT2;
  static constexpr int kApFailBit = BIT3;

  friend class WiFiAdapter;

  // --- STA ---

  void setStaConnected() {
    xEventGroupClearBits(group_, kStaFailBit);
    xEventGroupSetBits(group_, kStaConnectedBit);
  }

  void setStaDisconnected() {
    xEventGroupClearBits(group_, kStaConnectedBit);
    xEventGroupSetBits(group_, kStaFailBit);
  }


  // --- AP ---


  void setApConnected() {
    xEventGroupClearBits(group_, kApFailBit);
    xEventGroupSetBits(group_, kApConnectedBit);
  }

  void setApDisconnected() {
    xEventGroupClearBits(group_, kApConnectedBit);
    xEventGroupSetBits(group_, kApFailBit);
  }
};

}  // namespace app::network