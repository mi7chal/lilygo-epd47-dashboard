#include "DnsServer.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <cerrno>

#include "DnsPacket.hpp"
#include "logger.hpp"

namespace app::network {

DnsServer::DnsServer() = default;

DnsServer::~DnsServer() { stop(); }

bool DnsServer::start(IpAddress ip_address) {
  if (isActive()) {
    return true;
  }
  runner_.stop();  // no-op when Idle; waits out pending requestStop() or reaps Exited

  ip_address_ = ip_address;

  // priority 5 is equal to http priority, so dns and http won't starve each other
  // cpu core 1 (application) to not interfere with system tasks
  // change to core 0 seems also logical tho
  if (!runner_.start(&DnsServer::taskEntry, this, "dns_server_task", 4096, 5, 1)) {
    logger::error("Failed to create DNS server task");
    return false;
  }

  return true;
}

void DnsServer::stop() { runner_.stop(); }

void DnsServer::taskEntry(void* context) { static_cast<DnsServer*>(context)->runLoop(); }

void DnsServer::runLoop() {
  logger::info("DNS Server started");

  sockaddr_in dest_addr{};
  dest_addr.sin_addr.s_addr = dns::toNetworkOrder(static_cast<std::uint32_t>(INADDR_ANY));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = dns::toNetworkOrder(static_cast<std::uint16_t>(53));

  const int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (socket_fd < 0) {
    logger::error("DNS Socket initialization failed");
    return;
  }

  if (bind(socket_fd, reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr)) < 0) {
    logger::error("DNS Socket bind failed");
    close(socket_fd);
    return;
  }

  constexpr timeval kRecvTimeout{.tv_sec = 0, .tv_usec = 200000};
  if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &kRecvTimeout, sizeof(kRecvTimeout)) < 0) {
    logger::warn("DNS failed to set SO_RCVTIMEO: {}", errno);
  }

  // buffer for receiving the query and building the response in place
  // aligned to 4 bytes for easier parsing of the header and answer record
  alignas(std::uint32_t) std::array<std::uint8_t, 300> buffer{};

  while (!runner_.stopRequested()) {
    sockaddr_storage source_addr{};
    socklen_t socklen = sizeof(source_addr);

    // leave room to append the answer record
    const int len = recvfrom(socket_fd, buffer.data(), buffer.size() - dns::kAnswerSize, 0,
                             reinterpret_cast<struct sockaddr*>(&source_addr), &socklen);

    if (len < 0) {
      if (runner_.stopRequested()) {
        break;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      logger::error("DNS recvfrom failed: {}", errno);
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    const std::size_t response_len =
        dns::buildResponse(buffer.data(), static_cast<std::size_t>(len), ip_address_.to_u32());
    if (response_len == 0) {
      continue;  // not a query we can answer, ignore
    }

    sendto(socket_fd, buffer.data(), response_len, 0, reinterpret_cast<struct sockaddr*>(&source_addr), socklen);
  }

  close(socket_fd);
  logger::info("DNS Server stopped");
}

}  // namespace app::network
