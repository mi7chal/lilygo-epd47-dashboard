#pragma once

#include <esp_timer.h>

#include <cstdint>

namespace app::utils {

class Timer {
 public:
  using CallbackFunc = void (*)(void* context);

  Timer(void* context, CallbackFunc callback);
  ~Timer();

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  void startOnce(std::uint32_t timeout_ms);
  void stop();
  [[nodiscard]] bool isActive() const;

 private:
  esp_timer_handle_t timer_handle_{nullptr};
  CallbackFunc user_callback_{nullptr};
  void* context_{nullptr};
};

}  // namespace app::utils