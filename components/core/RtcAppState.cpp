#include "RtcAppState.hpp"

#include <esp_system.h>

#include "logger.hpp"

namespace app::core {

bool RtcAppState::isValid() const noexcept { return magic == kMagic; }

void RtcAppState::reset() noexcept {
  *this = {};
  magic = kMagic;
}

void RtcAppState::init() noexcept {
  if (!isValid() || esp_reset_reason() != ESP_RST_DEEPSLEEP) {
    logger::info("Cold boot or invalid RTC memory. Resetting RTC state.");
    reset();
  }
  boot_count++;
}

}  // namespace app::core
