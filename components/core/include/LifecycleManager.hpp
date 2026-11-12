#pragma once

#include "HardwareConfigFacade.hpp"
#include "NetworkManager.hpp"
#include "RtcAppState.hpp"
#include "SntpAdapter.hpp"
#include "State.hpp"
#include "StorageManager.hpp"


// app lifecycle:
//   - init hardware (esp event loop, netif, etc) - done
//   - check if button is pressed to force config mode - done
//   - init storage - done
//   - init network - done
//   - init and sync time
//   - mark config mode enabled if button pressed or wifi connection failed - done
//   - if in config mode, init portal and start it
//.  - if not in config mode, fetch weather and display it

// todo check if shouldn't sync geolocation be added here

namespace app::core {

enum class LifecycleState : uint8_t {
  // stable states
  Uninitialized = 0x00,
  Initialized = 0x01,
  RunningConfigMode = 0x02,
  Running = 0x03,
  Hibernated = 0x04,
  TornDown = 0x05,

  // transitional states
  Initializing = 0x40,
  StartingConfigMode = 0x41,
  Starting = 0x42,
  Hibernating = 0x43,
  TearingDown = 0x44
};

constexpr bool is_lifecycle_state_transitional(LifecycleState state) noexcept {
  return (static_cast<std::underlying_type_t<LifecycleState>>(state) & 0x40) != 0;
}

constexpr const char* to_string(LifecycleState state) noexcept {
  switch (state) {
    case LifecycleState::Uninitialized:
      return "Uninitialized";
    case LifecycleState::Initialized:
      return "Initialized";
    case LifecycleState::RunningConfigMode:
      return "RunningConfigMode";
    case LifecycleState::Running:
      return "Running";
    case LifecycleState::Hibernated:
      return "Hibernated";
    case LifecycleState::TornDown:
      return "TornDown";
    case LifecycleState::Initializing:
      return "Initializing";
    case LifecycleState::StartingConfigMode:
      return "StartingConfigMode";
    case LifecycleState::Starting:
      return "Starting";
    case LifecycleState::Hibernating:
      return "Hibernating";
    case LifecycleState::TearingDown:
      return "TearingDown";
  }
  return "Unknown";
}

constexpr const char* format_as(LifecycleState state) noexcept { return to_string(state); }


/**
 * @brief Mediator-like class that manages the lifecycle of the app
 *
 */
class LifecycleManager {
 public:
  explicit LifecycleManager(RtcAppState& rtc_state);
  ~LifecycleManager();

  void init();
  void run();
  void hibernate();
  void teardown();
  [[nodiscard]] LifecycleState getState() const { return state_.get(); }

  [[nodiscard]] RtcAppState& getRtcState() noexcept { return rtc_state_; }
  [[nodiscard]] const RtcAppState& getRtcState() const noexcept { return rtc_state_; }

 private:
  RtcAppState& rtc_state_;
  HardwareConfigFacade hardware_config_;
  storage::StorageManager storage_;
  network::NetworkManager network_;

  utils::State<LifecycleState, is_lifecycle_state_transitional> state_{LifecycleState::Uninitialized};
};

}  // namespace app::core
