#include "logger.hpp"

#include <fmt/args.h>
#include <fmt/chrono.h>

#include <chrono>


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

const char* tag_for(const std::source_location& location) {
  thread_local fmt::memory_buffer tag;  // memory buffer to hold the memory and reuse it across calls
  tag.clear();
  fmt::format_to(std::back_inserter(tag), "{}:{}", short_file_name(location.file_name()),
                 static_cast<unsigned>(location.line()));  // static cast to unsigned in order to avoid warnings
  tag.push_back('\0');
  return tag.data();
}

std::string_view local_time_view() {
  using namespace std::chrono;

  auto now = system_clock::now();

  constexpr auto kMinValidDate = sys_days{2024y / January / 1d};

  if (now < kMinValidDate) {
    return "--";
  }

  thread_local fmt::memory_buffer buffer;
  buffer.clear();

  fmt::format_to(std::back_inserter(buffer), "{:%d.%m.%Y %H:%M:%S}", now);

  return std::string_view(buffer.data(), buffer.size());
}

}  // namespace


void write_impl(LogLevel level, const std::source_location& location, fmt::string_view format, fmt::format_args args) {
  fmt::memory_buffer line;

  fmt::format_to(std::back_inserter(line), " [{}] ", local_time_view());
  fmt::vformat_to(std::back_inserter(line), format, args);

  line.push_back('\0');  // null-terminate the buffer for esp_log_write

  esp_log_write(static_cast<esp_log_level_t>(level), tag_for(location), "%s", line.data());
}

}  // namespace app::logger
