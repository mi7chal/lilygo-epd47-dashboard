#include "HardwareConfigFacade.hpp"

#include <esp_netif_sntp.h>
// #include <esp_sntp.h>
// #include <esp_system.h>
#include <driver/gpio.h>
#include <esp_attr.h>

#include <chrono>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "logger.hpp"


namespace app::core {

namespace {
constexpr gpio_num_t kConfigButtonPin = GPIO_NUM_21;
}  // namespace


// todo add deinit
// todo consider separating this class into multiple ones
HardwareConfigFacade::HardwareConfigFacade() = default;
HardwareConfigFacade::~HardwareConfigFacade() = default;

bool HardwareConfigFacade::initHardware() {
  const esp_err_t netifErr = esp_netif_init();
  if (netifErr != ESP_OK && netifErr != ESP_ERR_INVALID_STATE) {
    logger::error("esp_netif_init failed: {}", esp_err_to_name(netifErr));
    return false;
  }

  const esp_err_t eventLoopErr = esp_event_loop_create_default();
  if (eventLoopErr != ESP_OK && eventLoopErr != ESP_ERR_INVALID_STATE) {
    logger::error("esp_event_loop_create_default failed: {}", esp_err_to_name(eventLoopErr));
    return false;
  }

  logger::info("Hardware initialized");

  return true;
}

bool HardwareConfigFacade::isButtonPressed() {
  if (!button_initialized_) {
    constexpr gpio_num_t kConfigButtonPin = GPIO_NUM_21;
    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << kConfigButtonPin),
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    const esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
      logger::error("gpio_config for config button failed: {}", esp_err_to_name(err));
      return false;
    }

    logger::debug("Config button initialized on GPIO {}", static_cast<int>(kConfigButtonPin));

    button_initialized_ = true;
  }

  return gpio_get_level(GPIO_NUM_21) == 0;
}


void HardwareConfigFacade::syncTime() {
  // todo research and consider dhcp sntp
  if (isTimeSyncronized()) {
    logger::debug("Time is already synchronized, skipping sync");
  }

  // todo consider using multiple serveers and/or dhcp provided servers. also consider migrating to openweather ntp
  // todo handle errors, netif sntp init state etc.

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);
}


bool HardwareConfigFacade::isTimeSyncronized() const {
  using namespace std::chrono;
  auto now = system_clock::now();

  auto kMinValidDate = sys_days{2024y / January / 1d};

  return now >= kMinValidDate;
}


}  // namespace app::core
