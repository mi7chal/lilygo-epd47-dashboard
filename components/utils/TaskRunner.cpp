#include "TaskRunner.hpp"

namespace app::utils {

TaskRunner::TaskRunner() : done_(xSemaphoreCreateBinary()) {}

TaskRunner::~TaskRunner() {
  stop();
  vSemaphoreDelete(done_);
}

bool TaskRunner::start(Body body, void* context, const char* name, uint32_t stack_bytes, UBaseType_t priority,
                       BaseType_t core) {
  join();  // reap a task that ended on its own; no-op when idle
  if (!state_.transition(TaskState::Idle, TaskState::Running)) {
    return false;
  }

  body_ = body;
  context_ = context;
  stop_requested_ = false;

  if (xTaskCreatePinnedToCore(&TaskRunner::entry, name, stack_bytes, this, priority, nullptr, core) != pdPASS) {
    state_.set(TaskState::Idle);
    return false;
  }
  return true;
}

void TaskRunner::join() {
  if (state_.is(TaskState::Idle)) return;
  xSemaphoreTake(done_, portMAX_DELAY);
  state_.set(TaskState::Idle);
}

void TaskRunner::stop() {
  requestStop();
  join();
}

void TaskRunner::entry(void* self_ptr) {
  auto* self = static_cast<TaskRunner*>(self_ptr);
  self->body_(self->context_);
  self->state_.set(TaskState::Exited);
  xSemaphoreGive(self->done_);  // last touch of `self`: after this the owner may destroy it
  vTaskDelete(nullptr);
}

}  // namespace app::utils
