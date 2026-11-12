#pragma once

#include <atomic>
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
  void stop();
  [[nodiscard]] bool isActive() const { return runner_.isRunning(); }

 private:
  static void taskEntry(void* context);
  void runLoop();
  void closeSocket();

  utils::TaskRunner runner_;
  std::atomic<int> server_socket_{-1};
  IpAddress ip_address_;
};

}  // namespace app::network
