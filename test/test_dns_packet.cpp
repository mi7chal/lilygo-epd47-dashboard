#include <gtest/gtest.h>

#include <vector>

#include "DnsPacket.hpp"

using namespace app::network::dns;

// example.com question of the given type; optionally followed by an EDNS OPT record
static std::vector<std::uint8_t> makeQuery(std::uint16_t qtype, bool with_opt) {
  std::vector<std::uint8_t> q = {0x12,
                                 0x34,
                                 0x01,
                                 0x00,  // id, flags: RD
                                 0x00,
                                 0x01,
                                 0x00,
                                 0x00,
                                 0x00,
                                 0x00,
                                 0x00,
                                 static_cast<std::uint8_t>(with_opt ? 1 : 0),
                                 7,
                                 'e',
                                 'x',
                                 'a',
                                 'm',
                                 'p',
                                 'l',
                                 'e',
                                 3,
                                 'c',
                                 'o',
                                 'm',
                                 0,
                                 static_cast<std::uint8_t>(qtype >> 8),
                                 static_cast<std::uint8_t>(qtype & 0xFF),
                                 0,
                                 1};
  if (with_opt) {
    for (int b : {0, 0, 41, 16, 0, 0, 0, 0, 0, 0, 0}) q.push_back(static_cast<std::uint8_t>(b));  // OPT, size 4096
  }
  return q;
}

static std::size_t run(std::vector<std::uint8_t>& packet, std::uint32_t ip = 0x0104A8C0) {
  const std::size_t len = packet.size();
  packet.resize(len + kAnswerSize);
  return buildResponse(packet.data(), len, ip);
}

TEST(DnsPacketTest, BuildResponse) {
  constexpr std::size_t kQuestionEnd = 12 + 13 + 4;

  // A query
  auto a = makeQuery(1, false);
  EXPECT_EQ(kQuestionEnd + kAnswerSize, run(a));
  EXPECT_EQ(0x85, a[2]);
  EXPECT_EQ(0x00, a[3]);
  EXPECT_EQ(1, a[7]);
  AnswerRecord rec;
  std::memcpy(&rec, a.data() + kQuestionEnd, sizeof rec);
  EXPECT_EQ(0x0104A8C0u, rec.ip_address);

  // EDNS OPT is cut off
  auto edns = makeQuery(1, true);
  EXPECT_EQ(kQuestionEnd + kAnswerSize, run(edns));
  EXPECT_EQ(0, edns[11]);

  // AAAA gets an empty answer
  auto aaaa = makeQuery(28, false);
  EXPECT_EQ(kQuestionEnd, run(aaaa));
  EXPECT_EQ(0, aaaa[7]);

  // everything else is dropped
  std::vector<std::uint8_t> tiny(5, 0);
  EXPECT_EQ(0u, run(tiny));

  auto response_flag = makeQuery(1, false);
  response_flag[2] |= 0x80;
  EXPECT_EQ(0u, run(response_flag));

  auto truncated = makeQuery(1, false);
  truncated.resize(20);  // ends inside the name
  EXPECT_EQ(0u, run(truncated));

  auto two_questions = makeQuery(1, false);
  two_questions[5] = 2;
  EXPECT_EQ(0u, run(two_questions));

  auto pointer = makeQuery(1, false);
  pointer[12] = 0xC0;
  EXPECT_EQ(0u, run(pointer));
}
