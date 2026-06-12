#include "AppOrchestrator.hpp"

#include <driver/gpio.h>

#include "logger.hpp"

namespace app::core {

AppOrchestrator::AppOrchestrator() : hardware_config_(), storage_(), network_() {}

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

  auto wifi_info = network_.waitForWiFiConnectionInfo();  // todo don't wait if improper credentials

  if (!wifi_info.has_value()) {
    logger::warn("WiFi connection failed, entering configuration mode");
    is_config_mode = true;
  }


  // todo check for geocode sync required

  // todo init display

  // todo fetch weather and display it

  return is_config_mode;
}


}  // namespace app::core