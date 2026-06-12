#pragma once

#include <esp_log.h>
#include <fmt/core.h>

#include <source_location>
#include <utility>
#include <type_traits>

namespace app::logger {

// cusotm LogLevel enum to avoid direct depenedency and be future-proof
enum class LogLevel {
  Debug = ESP_LOG_DEBUG,
  Info = ESP_LOG_INFO,
  Warn = ESP_LOG_WARN,
  Error = ESP_LOG_ERROR,
  None = ESP_LOG_NONE,
};

void set_log_level(LogLevel level = LogLevel::Info);

namespace detail {
// hiding write_impl to keep the public interface hermetic
void write_impl(LogLevel level, const std::source_location& location, fmt::string_view format,
                fmt::format_args args = {});

template <typename... Args>
struct format_string_with_location {
  fmt::format_string<Args...> fmt;
  std::source_location loc;

  template <typename T>
  consteval format_string_with_location(
      const T& f,
      std::source_location loc = std::source_location::current()
  ) : fmt(f), loc(loc) {}
};

}  // namespace detail


template <LogLevel Level, typename... Args>
void log(detail::format_string_with_location<std::type_identity_t<Args>...> helper, Args&&... args) {
  detail::write_impl(Level, helper.loc, helper.fmt, fmt::make_format_args(args...));
}

template <typename... Args>
void error(detail::format_string_with_location<std::type_identity_t<Args>...> f, Args&&... a) {
  log<LogLevel::Error>(f, std::forward<Args>(a)...);
}

template <typename... Args>
void warn(detail::format_string_with_location<std::type_identity_t<Args>...> f, Args&&... a) {
  log<LogLevel::Warn>(f, std::forward<Args>(a)...);
}

template <typename... Args>
void info(detail::format_string_with_location<std::type_identity_t<Args>...> f, Args&&... a) {
  log<LogLevel::Info>(f, std::forward<Args>(a)...);
}

template <typename... Args>
void debug(detail::format_string_with_location<std::type_identity_t<Args>...> f, Args&&... a) {
  log<LogLevel::Debug>(f, std::forward<Args>(a)...);
}


}  // namespace app::logger
