#pragma once

#include <esp_attr.h>

#include <ctime>


namespace app::core {

class HardwareConfigFacade {
 public:
  HardwareConfigFacade();
  ~HardwareConfigFacade();

  bool initHardware();
  [[nodiscard]] bool isButtonPressed();
  void syncTime();
  [[nodiscard]] bool isTimeSynchronized() const;

 private:
  bool button_initialized_{false};
};

}  // namespace app::core