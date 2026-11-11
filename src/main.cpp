#include "sdkconfig.h"

#ifndef CONFIG_SPIRAM
#error "Please enable PSRAM in sdkconfig/menuconfig!"
#endif

#include <esp_attr.h>

#include "Application.hpp"
#include "LifecycleManager.hpp"
#include "RtcAppState.hpp"
#include "logger.hpp"

RTC_DATA_ATTR static app::core::RtcAppState rtc_app_state;

app::core::LifecycleManager lifecycle_manager{rtc_app_state};
app::core::Application application{lifecycle_manager};

extern "C" void app_main() {
  app::logger::set_log_level(app::logger::LogLevel::Info);
  app::logger::info("Application starting (boot count: {})...", rtc_app_state.getBootCount());

  application.run();
}
