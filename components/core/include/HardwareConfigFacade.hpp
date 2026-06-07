#pragma once

namespace app::core {

class HardwareConfigFacade {
 public:
  HardwareConfigFacade();
  ~HardwareConfigFacade();

  bool initHardware();
  [[nodiscard]] bool isButtonPressed();
  void syncTime();
  [[nodiscard]] bool isTimeSyncronized() const;

 private:
  bool button_initialized_{false};
};

}  // namespace app::core