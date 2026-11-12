#pragma once
// Host-side stub of esp_log.h: records output instead of printing to UART.

#include <cstdarg>
#include <cstdio>
#include <string>

typedef enum {
  ESP_LOG_NONE,
  ESP_LOG_ERROR,
  ESP_LOG_WARN,
  ESP_LOG_INFO,
  ESP_LOG_DEBUG,
  ESP_LOG_VERBOSE,
} esp_log_level_t;

namespace esp_log_stub {
inline esp_log_level_t level = ESP_LOG_INFO;
inline std::string last_output;
inline std::string last_tag;
inline int write_count = 0;
}  // namespace esp_log_stub

inline void esp_log_level_set(const char*, esp_log_level_t l) { esp_log_stub::level = l; }
inline esp_log_level_t esp_log_level_get(const char*) { return esp_log_stub::level; }

inline void esp_log_write(esp_log_level_t, const char* tag, const char* format, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  esp_log_stub::last_output = buf;
  esp_log_stub::last_tag = tag;
  ++esp_log_stub::write_count;
}

