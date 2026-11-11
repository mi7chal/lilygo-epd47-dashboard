#pragma once

#include <cstdint>
#include <ctime>
#include <type_traits>

namespace app::core {

/**
 * @brief Represents the state of the app that is stored in RTC memory
 *
 * @see https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/memory-types.html
 *
 */
class RtcAppState {
 public:
  RtcAppState() = default;
  void reset() noexcept;
  void init() noexcept;

  [[nodiscard]] bool isValid() const noexcept;
  [[nodiscard]] uint32_t getBootCount() const noexcept { return boot_count; }
  [[nodiscard]] time_t getLastSntpSync() const noexcept { return last_sntp_sync; }

 private:
  static constexpr uint32_t kMagic = 0x7B9A2F41;

  uint32_t magic;

  uint32_t boot_count;
  time_t last_sntp_sync;
  // uint16_t partial_refresh_count{0};
  // uint16_t last_battery_mv{0};
  // uint8_t consecutive_network_failures{0};
};

static_assert(std::is_trivially_copyable_v<RtcAppState>);
static_assert(std::is_trivially_default_constructible_v<RtcAppState>);

}  // namespace app::core
