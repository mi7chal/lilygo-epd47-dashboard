#pragma once

namespace app::core {

class HardwareConfigFacade {
 public:
  bool initHardware();
  [[nodiscard]] bool isButtonPressed();

 private:
  bool button_initialized_{false};
};

}  // namespace app::core