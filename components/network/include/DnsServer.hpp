#pragma once

#include <atomic>
#include <cstdint>

#include "IpAddress.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace app::network {

class DnsServer {
 public:
  DnsServer();
  ~DnsServer();

  DnsServer(const DnsServer&) = delete;
  DnsServer& operator=(const DnsServer&) = delete;
  DnsServer(DnsServer&&) = delete;
  DnsServer& operator=(DnsServer&&) = delete;

  bool start(IpAddress ip_address);
  void stop();
  [[nodiscard]] bool isActive() const { return is_running_; }

 private:
  static void taskWrapper(void* context);
  void runLoop();

  std::atomic<bool> is_running_{false};
  TaskHandle_t task_handle_{nullptr};
  SemaphoreHandle_t task_lifecycle_sem_{nullptr};
  int server_socket_{-1};
  IpAddress ip_address_;
};

}  // namespace app::network