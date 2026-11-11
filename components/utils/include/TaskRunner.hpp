#pragma once

#include <atomic>
#include <cstdint>

#include "State.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace app::utils {

/**
 * @brief Owns a FreeRTOS task and provides a simple interface to manage it
 */
class TaskRunner {
 public:
  using Body = void (*)(void* context);

  TaskRunner();
  ~TaskRunner();

  TaskRunner(const TaskRunner&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;
  TaskRunner(TaskRunner&&) = delete;
  TaskRunner& operator=(TaskRunner&&) = delete;


  bool start(Body body, void* context, const char* name, uint32_t stack_bytes, UBaseType_t priority,
             BaseType_t core = tskNO_AFFINITY);

  void requestStop() noexcept { stop_requested_ = true; }

  void join();

  void stop();

  [[nodiscard]] bool stopRequested() const noexcept { return stop_requested_; }
  [[nodiscard]] bool isRunning() const noexcept { return state_.is(TaskState::Running); }

 private:
  enum class TaskState : uint8_t { Idle, Running, Exited };

  static void entry(void* self_ptr);

  State<TaskState> state_{TaskState::Idle};
  std::atomic<bool> stop_requested_{false};
  SemaphoreHandle_t done_;
  Body body_{nullptr};
  void* context_{nullptr};
};

}  // namespace app::utils
