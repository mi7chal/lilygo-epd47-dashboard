#pragma once

#include <fmt/format.h>

#include <array>
#include <cstdint>

namespace app::network {

class IpAddress {
 private:
  std::array<uint8_t, 4> m_bytes{0, 0, 0, 0};

 public:
  constexpr IpAddress() = default;

  constexpr IpAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) : m_bytes{b1, b2, b3, b4} {}

  constexpr explicit IpAddress(uint32_t u32_addr) {
    m_bytes[0] = static_cast<uint8_t>(u32_addr & 0xFF);
    m_bytes[1] = static_cast<uint8_t>((u32_addr >> 8) & 0xFF);
    m_bytes[2] = static_cast<uint8_t>((u32_addr >> 16) & 0xFF);
    m_bytes[3] = static_cast<uint8_t>((u32_addr >> 24) & 0xFF);
  }

  constexpr uint8_t operator[](size_t index) const { return m_bytes.at(index); }

  constexpr uint32_t to_u32() const {
    return (static_cast<uint32_t>(m_bytes[3]) << 24) | (static_cast<uint32_t>(m_bytes[2]) << 16) |
           (static_cast<uint32_t>(m_bytes[1]) << 8) | static_cast<uint32_t>(m_bytes[0]);
  }

  constexpr bool operator==(const IpAddress& other) const { return m_bytes == other.m_bytes; }

  constexpr uint8_t operator[](size_t index) const { return m_bytes.at(index); }

  std::string to_string() const { return fmt::to_string(*this); }
};

}  // namespace app::network

/**
 * @brief formatter which allows using IpAddress directly in fmt::format and related functions
 *
 * @tparam
 */
template <>
struct fmt::formatter<app::network::IpAddress> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {}

  template <typename FormatContext>
  auto format(const app::network::IpAddress& ip, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]);
  }
};