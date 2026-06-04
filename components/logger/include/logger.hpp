#pragma once

#include <esp_log.h>
#include <fmt/core.h>

#include <source_location>

namespace app::logger {

// cusotm LogLevel enum to avoid direct depenedency and be future-proof
enum class LogLevel : int {
  Debug = ESP_LOG_DEBUG,
  Info = ESP_LOG_INFO,
  Warn = ESP_LOG_WARN,
  Error = ESP_LOG_ERROR,
  None = ESP_LOG_NONE,
};

void set_log_level(LogLevel level = LogLevel::Info);

// hiding write_impl (and other helper functions) to keep the public interface hermetic
namespace detail {

void write_impl(LogLevel level, const std::source_location& location, fmt::string_view format,
                fmt::format_args args = {});

}  // namespace detail

template <LogLevel Level, typename... Args>
void log(fmt::format_string<Args...> fmt_str, Args&&... args,
         const std::source_location& location = std::source_location::current()) {
  detail::write_impl(Level, location, fmt_str, fmt::make_format_args(args...));
}

template <typename... Args>
void error(fmt::format_string<Args...> f, Args&&... a) {
  log<LogLevel::Error>(f, std::forward<Args>(a)...);
}

template <typename... Args>
void warn(fmt::format_string<Args...> f, Args&&... a) {
  log<LogLevel::Warn>(f, std::forward<Args>(a)...);
}

template <typename... Args>
void info(fmt::format_string<Args...> f, Args&&... a) {
  log<LogLevel::Info>(f, std::forward<Args>(a)...);
}

template <typename... Args>
void debug(fmt::format_string<Args...> f, Args&&... a) {
  log<LogLevel::Debug>(f, std::forward<Args>(a)...);
}

}  // namespace app::logger
