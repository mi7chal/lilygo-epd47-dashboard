#pragma once

#include "HardwareConfigFacade.hpp"
#include "NetworkManager.hpp"
#include "StorageManager.hpp"


// app run procedure:
//   - init hardware (esp event loop, netif, etc) - done
//   - check if button is pressed to force config mode - done
//   - init storage - done
//   - init network - done
//   - init and sync time
//   - mark config mode enabled if button pressed or wifi connection failed - done
//   - if in config mode, init portal and start it
//.  - if not in config mode, fetch weather and display it

// todo check if shouldn't sync geolocation be added to this procedure

namespace app::core {

class AppOrchestrator {
 public:
  AppOrchestrator();
  ~AppOrchestrator();

  bool initApp();

 private:
  HardwareConfigFacade hardware_config_;
  storage::StorageManager storage_;
  network::NetworkManager network_;
};

}  // namespace app::core