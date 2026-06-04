#include "sdkconfig.h"
#ifndef CONFIG_SPIRAM
#error "Please enable PSRAM in sdkconfig/menuconfig!"
#endif

#include <string>
#include <fmt/format.h>

#include "ConfigManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "StorageManager.h"
#include "WebPortal.h"
#include "Log.h"
#include "TimeManager.h"

namespace {
  constexpr int SLEEP_DURATION_SECONDS = 30 * 60; // 30 minutes
}

StorageManager storage;
NetworkManager network;
WebPortal portal(storage);
TimeManager timeManager(storage);

ConfigManager config(storage, network, portal);
DisplayManager display;

extern "C" void app_main() {
  logger::set_log_level();

  logger::info("setup start");

  // initialize display and show loading screen
  display.init();


  logger::info("display init complete");

  // load our dashboard base config needed to fetch data
  // ensure storage loads persisted values from NVS before config initialization
  storage.init();
  auto configState = config.init();  


  logger::info("config init complete");
  // Now that network has been initialized by ConfigManager, start NTP and logger local time
  double lat = 0.0;
  double lon = 0.0;
  if (!storage.getLatitude().empty()) lat = std::stod(storage.getLatitude());
  if (!storage.getLongitude().empty()) lon = std::stod(storage.getLongitude());
  timeManager.begin();
  std::string localTime = timeManager.getLocalTimeString(lat, lon);
  if (localTime.empty()) {
    logger::warn("Local time not ready yet");
  } else {
    logger::info("Local time: {}", localTime.c_str());
  }
  logger::info("Initialization complete!");

   bool showConfigurationScreen = false;

   std::visit([&](const auto& state) {
    using T = std::decay_t<decltype(state)>;
    std::string message;

    if constexpr (std::is_same_v<T, APConfigState>) {
      logger::info("APConfigState selected");
        showConfigurationScreen = true;
      message = fmt::format(
        "Connect to WiFi network \"{}\" with password \"{}\" to configure the device.",
        state.ssid,
        state.password);
    } else if constexpr (std::is_same_v<T, STAConfigState>) {
      logger::info("STAConfigState selected");
        showConfigurationScreen = true;
      message = fmt::format(
        "Device is connected to WiFi network \"{}\" but configuration button was pressed. Connect to the same WiFi network to configure the device under \"{}.local\".",
        state.ssid,
        state.hostname);
      } else {
        logger::info("NormalState selected");
    }

      if (showConfigurationScreen) {
        display.showCustomScreen(CustomScreenData{
            "Configuration Mode",
            message
        });

        // Zamiast duplikować cały tekst, używamy wygenerowanej zmiennej
        logger::info("Configuration mode enabled. {}", message);
      }

}, configState);

  if (showConfigurationScreen) {
    return;
  }

  display.showDashboard(DashboardData{
    "Partly cloudy",
    "Warsaw",
    20.5F,
    60U,
    1013U,
    5.5F});

  display.goToSleep(SLEEP_DURATION_SECONDS);
  
}

void loop() {
  if(config.isConfigMode()) {
    portal.processDNS();
  }
}