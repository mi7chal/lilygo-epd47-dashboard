#include "AppOrchestrator.hpp"

#include "driver/gpio.h"
#include "logger.hpp"

namespace app::core {

AppOrchestrator::AppOrchestrator() : storage_(), network_(), hardware_config_() {}

AppOrchestrator::~AppOrchestrator() = default;

// todo add more extensive error handling
bool AppOrchestrator::initApp() {
  hardware_config_.initHardware();

  bool is_config_mode = hardware_config_.isButtonPressed();
  if (is_config_mode) {
    logger::info("Configuration button pressed, configuration mode is to be enabled");
  }

  storage_.init();
  network_.init();

  network_.connectToWiFi(storage_.getSSID(), storage_.getPassword());  // todo handle empty ssid case

  hardware_config_.syncTime();

  auto wifi_info = network_.waitForWiFiConnectionInfo();

  if (!wifi_info.has_value()) {
    logger::warn("WiFi connection failed, entering configuration mode");
    is_config_mode = true;
  }

  // todo check for geocode sync required

  // todo init display

  // todo fetch weather and display it

  return true;
}


}  // namespace app::core