#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>

#include "ConfigManager.h"
#include "DisplayManager.h"

constexpr int SLEEP_DURATION_SECONDS = 30 * 60; // 30 minutes

StorageManager storage;
NetworkManager network;
CaptivePortal portal(storage);

ConfigManager config(storage, network, portal);
DisplayManager display;

void setup() {
  Serial.begin(115200);

  // initialize display and show loading screen
  display.init();
  display.showLoadingScreen();

  // load our dashboard base config needed to fetch data
  config.init();
  
  Serial.println("Initialization complete!");

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
  // ignored
}