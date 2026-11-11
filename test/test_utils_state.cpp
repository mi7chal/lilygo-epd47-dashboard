#include <gtest/gtest.h>

#include <array>

#include "State.hpp"

enum class TestState : uint8_t { Uninitialized, Initialized, Running, Stopped };

TEST(UtilsStateTest, Transitions) {
  using app::utils::State;

  State<TestState> state(TestState::Uninitialized);

  // 1. Literal / rvalue transition
  EXPECT_TRUE(state.transition(TestState::Uninitialized, TestState::Initialized));
  EXPECT_TRUE(state.is(TestState::Initialized));

  // 2. Reference / lvalue transition (updates 'expected' on failure)
  TestState expected = TestState::Uninitialized;
  EXPECT_FALSE(state.transition(expected, TestState::Running));
  EXPECT_EQ(TestState::Initialized, expected);

  // Succeeds with updated expected value
  EXPECT_TRUE(state.transition(expected, TestState::Running));
  EXPECT_TRUE(state.is(TestState::Running));

  // 3. Initializer list transition
  EXPECT_TRUE(state.transition({TestState::Uninitialized, TestState::Running}, TestState::Stopped));
  EXPECT_TRUE(state.is(TestState::Stopped));

  // 4. std::array transition
  std::array<TestState, 2> valid_states{TestState::Stopped, TestState::Running};
  EXPECT_TRUE(state.transition(valid_states, TestState::Initialized));
  EXPECT_TRUE(state.is(TestState::Initialized));
}
