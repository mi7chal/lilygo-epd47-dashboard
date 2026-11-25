#include "Timer.hpp"


namespace app::utils {

Timer::Timer(void* context, CallbackFunc callback)
    : done_sem_(xSemaphoreCreateBinary()), user_callback_(callback), context_(context) {
  // ensure null callback is handled gracefully
  if (callback == nullptr) {
    user_callback_ = [](void* /*ctx*/) {};
  }
  if (done_sem_ == nullptr) {
    return;
  }

  esp_timer_create_args_t args = {
      .callback =
          [](void* arg) {
            auto* self = static_cast<Timer*>(arg);
            if (!self->cancelled_.load(std::memory_order_acquire) && self->user_callback_) {
              self->user_callback_(self->context_);
            }
            xSemaphoreGive(self->done_sem_);  // last touch of `self`: after this ~Timer() may destroy it
          },
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "app_timer",
      .skip_unhandled_events = false};
  if (esp_timer_create(&args, &timer_handle_) != ESP_OK) {
    timer_handle_ = nullptr;
  }
}

Timer::~Timer() {
  stop();

  if (timer_handle_ != nullptr) {
    esp_timer_delete(timer_handle_);
    timer_handle_ = nullptr;
  }
  if (done_sem_ != nullptr) {
    vSemaphoreDelete(done_sem_);
    done_sem_ = nullptr;
  }
}

void Timer::startOnce(std::uint32_t timeout_ms) {
  if (timer_handle_ == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(api_mutex_);
  stopLocked();
  cancelled_.store(false, std::memory_order_release);
  if (esp_timer_start_once(timer_handle_, static_cast<std::uint64_t>(timeout_ms) * 1000U) == ESP_OK) {
    armed_ = true;
  }
}

void Timer::stopLocked() {
  if (timer_handle_ == nullptr || !armed_) {
    return;
  }
  cancelled_.store(true, std::memory_order_release);
  armed_ = false;
  if (esp_timer_stop(timer_handle_) != ESP_OK) {
    xSemaphoreTake(done_sem_, portMAX_DELAY);
  }
}

void Timer::stop() {
  std::lock_guard<std::mutex> lock(api_mutex_);
  stopLocked();
}

bool Timer::isActive() const { return timer_handle_ != nullptr && esp_timer_is_active(timer_handle_); }
}  // namespace app::utils
