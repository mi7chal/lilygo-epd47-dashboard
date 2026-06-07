#include "sdkconfig.h"

#ifndef CONFIG_SPIRAM
#error "Please enable PSRAM in sdkconfig/menuconfig!"
#endif

#include "AppCore.hpp"
#include "AppOrchestrator.hpp"
#include "logger.hpp"

app::core::AppOrchestrator orchestrator{};
app::core::AppCore core{orchestrator};

void app_core_task_f(void* context) {
  auto* core = static_cast<app::core::AppCore*>(context);
  core->run();
  vTaskDelete(nullptr);  // Delete the task when done
}

extern "C" void app_main() {
  app::logger::set_log_level(app::logger::LogLevel::Info);
  app::logger::info("System starting...");

  xTaskCreatePinnedToCore(app_core_task_f, "app_core_task", 8192, &core, 2, nullptr, 1);
}
