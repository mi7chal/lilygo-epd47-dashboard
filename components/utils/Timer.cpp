#include "Timer.hpp"


namespace app::utils {

Timer::Timer(void* context, CallbackFunc callback) : user_callback_(callback), context_(context) {
  // ensure null callback is handled gracefully
  if (callback == nullptr) {
    user_callback_ = [](void* ctx) { /* Empty */ };
  }

  esp_timer_create_args_t args = {.callback =
                                      [](void* arg) {
                                        auto* self = static_cast<Timer*>(arg);
                                        if (self->user_callback_) {
                                          self->user_callback_(self->context_);
                                        }
                                      },
                                  .arg = this,
                                  .name = "app_timer"};
  esp_timer_create(&args, &timer_handle_);
}

Timer::~Timer() {
  stop();

  if (timer_handle_) {
    esp_timer_delete(timer_handle_);
  }
}


void Timer::startOnce(std::uint32_t timeout_ms) {
  stop();
  esp_timer_start_once(timer_handle_, static_cast<std::uint64_t>(timeout_ms) * 1000U);
}

void Timer::stop() {
  if (timer_handle_ && esp_timer_is_active(timer_handle_)) {
    esp_timer_stop(timer_handle_);
  }
}

bool Timer::isActive() const { return timer_handle_ && esp_timer_is_active(timer_handle_); }
}  // namespace app::utils