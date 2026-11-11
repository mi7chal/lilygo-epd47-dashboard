#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Pure packet logic of the captive portal DNS server, kept free of ESP-IDF headers so it can be tested on the host.
namespace app::network::dns {

constexpr std::uint16_t toNetworkOrder(std::uint16_t value) noexcept {
  if constexpr (std::endian::native == std::endian::big) {
    return value;
  } else {
    return std::byteswap(value);
  }
}

constexpr std::uint32_t toNetworkOrder(std::uint32_t value) noexcept {
  if constexpr (std::endian::native == std::endian::big) {
    return value;
  } else {
    return std::byteswap(value);
  }
}

struct __attribute__((packed)) Header {
  std::uint16_t id;
  std::uint16_t flags;
  std::uint16_t qd_count;
  std::uint16_t an_count;
  std::uint16_t ns_count;
  std::uint16_t ar_count;
};

struct __attribute__((packed)) AnswerRecord {
  std::uint16_t name_ptr;
  std::uint16_t type;
  std::uint16_t dns_class;
  std::uint32_t ttl;
  std::uint16_t data_len;
  std::uint32_t ip_address;
};

constexpr std::size_t kAnswerSize = sizeof(AnswerRecord);

/**
 * @brief Turns a DNS query into a response in place: header + the original question + one A record.
 *
 * Only plain single-question queries are answered. Anything after the question (e.g. the EDNS OPT record sent by
 * most phones) is cut off, so the answer always directly follows the question. Non-A questions (e.g. AAAA) get an
 * empty answer, which tells the client to fall back to IPv4.
 *
 * @param buf query on input, response on output; needs room for `len + kAnswerSize` bytes
 * @param len query length in bytes
 * @param ip_net_order IPv4 address as laid out in memory (what IpAddress::to_u32 returns on little-endian)
 * @return response length in bytes, 0 if the packet is not a query we can answer and must be dropped
 */
inline std::size_t buildResponse(std::uint8_t* buf, std::size_t len, std::uint32_t ip_net_order) noexcept {
  if (len < sizeof(Header)) {
    return 0;
  }

  Header header;
  std::memcpy(&header, buf, sizeof(Header));

  const std::uint16_t flags = toNetworkOrder(header.flags);
  const bool is_query = (flags & 0x8000) == 0;
  const std::uint16_t opcode = (flags >> 11) & 0xF;

  if (!is_query || opcode != 0 || toNetworkOrder(header.qd_count) != 1) {
    return 0;
  }

  // skip QNAME
  std::size_t pos = sizeof(Header);
  while (true) {
    if (pos >= len) {
      return 0;
    }
    const std::uint8_t label_len = buf[pos++];
    if (label_len == 0) {
      break;
    }
    if (label_len > 63) {  // compression pointer, invalid in a question
      return 0;
    }
    pos += label_len;
  }

  if (pos + 4 > len) {
    return 0;
  }
  const std::uint16_t qtype = static_cast<std::uint16_t>((buf[pos] << 8) | buf[pos + 1]);
  pos += 4;

  const bool answer = qtype == 1;  // A record

  header.flags = toNetworkOrder(static_cast<std::uint16_t>(0x8400 | (flags & 0x0100)));  // response + auth, keep RD
  header.an_count = toNetworkOrder(static_cast<std::uint16_t>(answer ? 1 : 0));
  header.ns_count = 0;
  header.ar_count = 0;
  std::memcpy(buf, &header, sizeof(Header));

  if (!answer) {
    return pos;
  }

  const AnswerRecord record{.name_ptr = toNetworkOrder(static_cast<std::uint16_t>(0xC000 | sizeof(Header))),
                            .type = toNetworkOrder(static_cast<std::uint16_t>(1)),
                            .dns_class = toNetworkOrder(static_cast<std::uint16_t>(1)),
                            .ttl = toNetworkOrder(static_cast<std::uint32_t>(10)),
                            .data_len = toNetworkOrder(static_cast<std::uint16_t>(4)),
                            .ip_address = ip_net_order};
  std::memcpy(buf + pos, &record, sizeof(AnswerRecord));
  return pos + sizeof(AnswerRecord);
}

}  // namespace app::network::dns
