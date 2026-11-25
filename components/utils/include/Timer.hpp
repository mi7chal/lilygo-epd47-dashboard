#pragma once

#include <esp_timer.h>

#include <atomic>
#include <cstdint>
#include <mutex>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace app::utils {

// Do not call startOnce() or stop() from inside the timer callback.
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
  void stopLocked();

  esp_timer_handle_t timer_handle_{nullptr};
  SemaphoreHandle_t done_sem_{nullptr};
  CallbackFunc user_callback_{nullptr};
  void* context_{nullptr};
  std::mutex api_mutex_;
  bool armed_{false};
  std::atomic<bool> cancelled_{false};
};

}  // namespace app::utils
