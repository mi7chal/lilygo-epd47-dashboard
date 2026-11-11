#include "LifecycleManager.hpp"

#include <driver/gpio.h>

#include "logger.hpp"

namespace app::core {

LifecycleManager::LifecycleManager(RtcAppState& rtc_state)
    : rtc_state_(rtc_state), hardware_config_(), storage_(), network_() {}

LifecycleManager::~LifecycleManager() = default;

// todo add more extensive error handling
void LifecycleManager::init() {
  LifecycleState expected = LifecycleState::Uninitialized;
  if (!state_.transition(expected, LifecycleState::Initializing)) {
    logger::warn("LifecycleManager::init() called in non-uninitialized state: {}", expected);
    return;
  }

  logger::debug("Starting initialization (boot count: {}, last sync: {})", rtc_state_.getBootCount(),
                rtc_state_.getLastSntpSync());

  rtc_state_.init();

  hardware_config_.initHardware();

  bool is_config_mode = hardware_config_.isButtonPressed();
  if (is_config_mode) {
    logger::info("Configuration button pressed, configuration mode is to be enabled");
  }

  storage_.init();
  network_.init();

  network_.connectToWiFi(storage_.getSSID(), storage_.getPassword());  // todo handle empty ssid case

  auto wifi_info = network_.waitForWiFiConnectionInfo();  // todo don't wait if improper credentials

  if (!wifi_info.has_value()) {
    logger::warn("WiFi connection failed, entering configuration mode");
    is_config_mode = true;
  }

  // todo check for geocode sync required

  // todo init display

  // todo fetch weather and display it

  state_.set(LifecycleState::Initialized);

  logger::info("Initialized successfully, configuration mode: {}", is_config_mode);
}

void LifecycleManager::run() {
  LifecycleState expected = LifecycleState::Initialized;
  if (!state_.transition(expected, LifecycleState::Starting)) {
    logger::warn("LifecycleManager::run() called in non-initialized state: {}", expected);
    return;
  }

  logger::debug("Starting app...");

  // todo start app

  state_.set(LifecycleState::Running);

  logger::info("App started successfully");
}

void LifecycleManager::hibernate() {
  if (!state_.transition({LifecycleState::Running, LifecycleState::RunningConfigMode, LifecycleState::Initialized},
                         LifecycleState::Hibernating)) {
    logger::warn("LifecycleManager::hibernate() called in non-running state: {}", state_.get());
    return;
  }

  logger::debug("Entering deep sleep mode...");

  // todo hibernate app

  state_.set(LifecycleState::Hibernated);

  logger::info("Entered deep sleep mode successfully");
}

void LifecycleManager::teardown() {
  if (!state_.transition({LifecycleState::Running, LifecycleState::RunningConfigMode, LifecycleState::Initialized,
                          LifecycleState::Hibernated},
                         LifecycleState::TearingDown)) {
    logger::warn("LifecycleManager::teardown() called in non-running state: {}", state_.get());
    return;
  }

  logger::debug("Tearing down app...");

  // todo teardown app

  state_.set(LifecycleState::TornDown);

  logger::info("App torn down successfully");
}

}  // namespace app::core