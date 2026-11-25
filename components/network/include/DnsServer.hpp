#pragma once

#include <cstdint>

#include "IpAddress.hpp"
#include "TaskRunner.hpp"

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
  void requestStop() noexcept { runner_.requestStop(); }
  void stop();
  [[nodiscard]] bool isActive() const { return runner_.isRunning() && !runner_.stopRequested(); }

 private:
  static void taskEntry(void* context);
  void runLoop();

  utils::TaskRunner runner_;
  IpAddress ip_address_;
};

}  // namespace app::network
