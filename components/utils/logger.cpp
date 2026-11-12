#include "logger.hpp"

#include <fmt/chrono.h>

#include <chrono>
#include <cstring>
#include <ctime>


namespace app::logger {

namespace {
const char* short_file_name(const char* path) {
  const char* last_slash = path;
  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') {
      last_slash = cursor + 1;
    }
  }

  return last_slash;
}

}  // namespace

void set_log_level(LogLevel level) { esp_log_level_set("*", static_cast<esp_log_level_t>(level)); }

namespace detail {

void write_impl(LogLevel level, const std::source_location& location, fmt::string_view format, fmt::format_args args) {
  char tag_buf[64];
  auto tag_res = fmt::format_to_n(tag_buf, sizeof(tag_buf) - 1, "{}:{}", short_file_name(location.file_name()),
                                  static_cast<unsigned>(location.line()));
  *tag_res.out = '\0';

  if (esp_log_level_get(tag_buf) < static_cast<esp_log_level_t>(level)) return;

  char time_buf[32];
  using namespace std::chrono;
  auto now = system_clock::now();
  constexpr auto kMinValidDate = sys_days{2024y / January / 1d};
  if (now < kMinValidDate) {
    std::strcpy(time_buf, "--");
  } else {
    std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm;
    localtime_r(&t, &local_tm);
    auto time_res = fmt::format_to_n(time_buf, sizeof(time_buf) - 1, "{:%d.%m.%Y %H:%M:%S}", local_tm);
    *time_res.out = '\0';
  }

  const char letter = level == LogLevel::Error  ? 'E'
                      : level == LogLevel::Warn ? 'W'
                      : level == LogLevel::Info ? 'I'
                                                : 'D';

  fmt::memory_buffer line;
  fmt::format_to(std::back_inserter(line), "{} [{}] {}: ", letter, time_buf, tag_buf);
  fmt::vformat_to(std::back_inserter(line), format, args);
  line.push_back('\n');
  line.push_back('\0');  // null-terminate the buffer for esp_log_write

  esp_log_write(static_cast<esp_log_level_t>(level), tag_buf, "%s", line.data());
}

}  // namespace detail

}  // namespace app::logger
