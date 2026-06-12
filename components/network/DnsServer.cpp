#include "DnsServer.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <bit>

#include "logger.hpp"

namespace app::network {

namespace {

constexpr std::uint16_t to_network_order(std::uint16_t value) noexcept {
  if constexpr (std::endian::native == std::endian::big) {
    return value;
  } else {
    return std::byteswap(value);
  }
}

constexpr std::uint32_t to_network_order(std::uint32_t value) noexcept {
  if constexpr (std::endian::native == std::endian::big) {
    return value;
  } else {
    return std::byteswap(value);
  }
}

struct __attribute__((packed)) DnsHeader {
  std::uint16_t id;
  std::uint16_t flags;
  std::uint16_t qd_count;
  std::uint16_t an_count;
  std::uint16_t ns_count;
  std::uint16_t ar_count;
};

struct __attribute__((packed)) DnsAnswerRecord {
  std::uint16_t name_ptr;
  std::uint16_t type;
  std::uint16_t dns_class;
  std::uint32_t ttl;
  std::uint16_t data_len;
  std::uint32_t ip_address;
};

struct DnsResponse {
  DnsHeader header;
  DnsAnswerRecord answer;

  DnsResponse(DnsHeader query_header, std::uint32_t ip_net_order) noexcept {
    header = query_header;
    header.flags = to_network_order(static_cast<std::uint16_t>(0x8400));  // Response + Auth
    header.an_count = to_network_order(static_cast<std::uint16_t>(1));    // 1 Answer

    answer = {.name_ptr = to_network_order(static_cast<std::uint16_t>(0xC000 | sizeof(DnsHeader))),
              .type = to_network_order(static_cast<std::uint16_t>(0x0001)),       // Type A
              .dns_class = to_network_order(static_cast<std::uint16_t>(0x0001)),  // Class IN
              .ttl = to_network_order(static_cast<std::uint32_t>(10)),            // TTL 10s
              .data_len = to_network_order(static_cast<std::uint16_t>(4)),        // IPv4
              .ip_address = ip_net_order};
  }
};

}  // namespace


DnsServer::DnsServer() { task_lifecycle_sem_ = xSemaphoreCreateBinary(); };

DnsServer::~DnsServer() {
  stop();

  if (task_lifecycle_sem_ != nullptr) {
    vSemaphoreDelete(task_lifecycle_sem_);
  }
}

bool DnsServer::start(IpAddress ip_address) {
  if (is_running_) {
    return true;
  }

  ip_address_ = ip_address;
  is_running_ = true;  // set this before starting the task in order to avoid it exiting immediately

  // create the task
  // priority 5 is equal to http priority, so dns and http won't starve each other
  // cpu core 1 (application) to not interfere with system tasks
  // change to core 0 seems also wise tho
  BaseType_t ret = xTaskCreatePinnedToCore(DnsServer::taskWrapper, "dns_server_task", 4096, this, 5, &task_handle_, 1);
  if (ret != pdPASS) {
    is_running_ = false;
    logger::error("Failed to create DNS server task");
    return false;
  }

  return true;
}

void DnsServer::stop() {
  if (!is_running_.exchange(false)) {
    return;
  }

  // close the socket to unblock the task if it's waiting on recvfrom
  if (server_socket_ != -1) {
    close(server_socket_);
    server_socket_ = -1;
  }

  // wait for the task to signal that it's exiting and then delete it
  if (task_handle_ != nullptr) {
    xSemaphoreTake(task_lifecycle_sem_, portMAX_DELAY);
    vTaskDelete(task_handle_);
    task_handle_ = nullptr;
  }

  logger::info("DNS Server stopped");
}

void DnsServer::taskWrapper(void* context) {
  auto* dns = static_cast<DnsServer*>(context);
  dns->runLoop();  // run the main loop of the DNS server, this will block until the server is stopped
  xSemaphoreGive(dns->task_lifecycle_sem_);  // signal that the task is exiting
  vTaskDelay(portMAX_DELAY);                 // wait indefinitely for the task to be deleted
}

void DnsServer::runLoop() {
  logger::info("DNS Server started");

  // prepare the destination address (bind to all interfaces on port 53 - dns)
  struct sockaddr_storage dest_addr{};
  auto* dest_addr_ip4 = reinterpret_cast<struct sockaddr_in*>(&dest_addr);
  dest_addr_ip4->sin_addr.s_addr = to_network_order(INADDR_ANY);
  dest_addr_ip4->sin_family = AF_INET;
  dest_addr_ip4->sin_port = to_network_order(static_cast<std::uint16_t>(53));

  // create the UDP socket and bind to port 53
  server_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (server_socket_ < 0 ||
      bind(server_socket_, reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr)) < 0) {
    logger::error("DNS Socket initialization failed");

    if (server_socket_ != -1) {
      close(server_socket_);
      server_socket_ = -1;
    }
    is_running_ = false;
    return;
  }


  // buffer for receiving and sending DNS packets
  // aligned to 4 bytes for easier parsing of the header and answer record
  alignas(std::uint32_t) std::array<std::uint8_t, 300> rx_buffer{};

  while (is_running_) {
    // prepare structures to receive the query
    struct sockaddr_storage source_addr{};
    socklen_t socklen = sizeof(source_addr);

    // receive the query (blocking)
    const int len = recvfrom(server_socket_, rx_buffer.data(), rx_buffer.size(), 0,
                             reinterpret_cast<struct sockaddr*>(&source_addr), &socklen);

    if (len < 0) {
      if (!is_running_) {
        break;
      }
      logger::error("DNS recvfrom failed: {}", errno);
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (len < static_cast<int>(sizeof(DnsHeader))) {
      continue;  // not a valid DNS packet, ignore
    }

    // parse the DNS header
    DnsHeader query_header{};
    std::memcpy(&query_header, rx_buffer.data(), sizeof(DnsHeader));

    // check if dns header is marked as a query, otherwise it should be ignored
    const bool is_query = (to_network_order(query_header.flags) & 0x8000) == 0;

    if (!is_query) {
      continue;  // not a query, ignore
    }

    if (static_cast<std::size_t>(len) + sizeof(DnsAnswerRecord) > rx_buffer.size()) [[unlikely]] {
      continue;  // buffer overflow, ignore this packet
    }

    const DnsResponse response(query_header, ip_address_.to_u32());

    // write the response directly into the rx buffer
    std::memcpy(rx_buffer.data(), &response.header, sizeof(DnsHeader));
    std::memcpy(rx_buffer.data() + len, &response.answer, sizeof(DnsAnswerRecord));

    // pack and send the response
    const std::size_t total_len = len + sizeof(DnsAnswerRecord);
    sendto(server_socket_, rx_buffer.data(), total_len, 0, reinterpret_cast<struct sockaddr*>(&source_addr), socklen);
  }
}

}  // namespace app::network