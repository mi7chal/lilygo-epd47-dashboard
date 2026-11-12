#include <gtest/gtest.h>

// test_build_src = no, so compile the implementation directly (esp_log.h is stubbed in test/stubs).
#include "../components/utils/logger.cpp"

namespace logger = app::logger;
namespace stub = esp_log_stub;

class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    stub::last_output.clear();
    stub::last_tag.clear();
    stub::write_count = 0;
    logger::set_log_level(logger::LogLevel::Debug);
  }
};

TEST_F(LoggerTest, FormatsArgumentsAndAddsLevelLetter) {
  logger::info("value={} name={}", 42, "x");
  EXPECT_EQ(1, stub::write_count);
  EXPECT_EQ('I', stub::last_output[0]);
  EXPECT_NE(std::string::npos, stub::last_output.find("value=42 name=x"));
  EXPECT_EQ('\n', stub::last_output.back());
}

TEST_F(LoggerTest, LevelLetters) {
  logger::error("e");
  EXPECT_EQ('E', stub::last_output[0]);
  logger::warn("w");
  EXPECT_EQ('W', stub::last_output[0]);
  logger::debug("d");
  EXPECT_EQ('D', stub::last_output[0]);
}

TEST_F(LoggerTest, TagIsShortFileAndLine) {
  logger::info("hi");
  EXPECT_EQ(0u, stub::last_tag.find("test_utils_logger.cpp:"));
  EXPECT_NE(std::string::npos, stub::last_output.find("test_utils_logger.cpp:"));
}

TEST_F(LoggerTest, FiltersBelowConfiguredLevel) {
  logger::set_log_level(logger::LogLevel::Warn);
  logger::debug("hidden");
  logger::info("hidden");
  EXPECT_EQ(0, stub::write_count);
  logger::warn("shown");
  logger::error("shown");
  EXPECT_EQ(2, stub::write_count);
}

TEST_F(LoggerTest, NoneSuppressesEverything) {
  logger::set_log_level(logger::LogLevel::None);
  logger::error("hidden");
  EXPECT_EQ(0, stub::write_count);
}

TEST_F(LoggerTest, TimestampIsLocalAndWellFormed) {
  logger::info("t");
  // Clock is valid on the host, so expect "dd.mm.yyyy hh:mm:ss" between brackets.
  auto open = stub::last_output.find('[');
  auto close = stub::last_output.find(']');
  ASSERT_NE(std::string::npos, open);
  ASSERT_NE(std::string::npos, close);
  EXPECT_EQ(19u, close - open - 1);
}

TEST_F(LoggerTest, ShortFileName) {
  EXPECT_STREQ("c.cpp", logger::short_file_name("/a/b/c.cpp"));
  EXPECT_STREQ("c.cpp", logger::short_file_name("a\\b\\c.cpp"));
  EXPECT_STREQ("c.cpp", logger::short_file_name("c.cpp"));
}

