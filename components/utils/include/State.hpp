#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <thread>
#include <type_traits>

namespace app::utils {

/**
 * @brief State class that represents a state for a given enum, waiting, all utils etc.
 *
 * @tparam StateEnum the enum with state
 */
template <typename StateEnum, bool (*IsTransitional)(StateEnum) = nullptr>
  requires std::is_enum_v<StateEnum>
class State {
 public:
  static_assert(sizeof(StateEnum) <= sizeof(uint32_t),
                "StateEnum must fit natively in atomic instructions (<= 32-bit)");

  constexpr explicit State(StateEnum initial_state) noexcept : state_(initial_state) {}

  ~State() = default;

  State(const State&) = delete;
  State& operator=(const State&) = delete;
  State(State&&) = delete;
  State& operator=(State&&) = delete;

  [[nodiscard]] StateEnum get() const noexcept { return state_.load(std::memory_order_acquire); }
  [[nodiscard]] bool is(StateEnum expected) const noexcept { return get() == expected; }

  [[nodiscard]] bool is_transitional() const noexcept {
    if constexpr (IsTransitional != nullptr) {
      return IsTransitional(get());
    } else {
      return false;
    }
  }

  [[nodiscard]] bool is_stable() const noexcept { return !is_transitional(); }

  void set(StateEnum new_state) noexcept { state_.store(new_state, std::memory_order_release); }

  bool transition(StateEnum& expected, StateEnum desired) noexcept {
    return state_.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire);
  }

  bool transition(StateEnum&& expected, StateEnum desired) noexcept { return transition(expected, desired); }

  bool transition(std::span<const StateEnum> expected_states, StateEnum desired) noexcept {
    StateEnum current = state_.load(std::memory_order_acquire);
    while (std::ranges::find(expected_states, current) != expected_states.end()) {
      if (state_.compare_exchange_weak(current, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  bool transition(std::initializer_list<StateEnum> expected_states, StateEnum desired) noexcept {
    return transition(std::span<const StateEnum>(expected_states.begin(), expected_states.end()), desired);
  }

  /// Waits until `pred(state)` is true. Returns false on timeout. Default timeout waits forever.
  template <std::predicate<StateEnum> Pred>
  bool wait_until(Pred pred, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) const noexcept {
    const auto start = std::chrono::steady_clock::now();
    while (!pred(get())) {
      if (timeout != std::chrono::milliseconds::max() && (std::chrono::steady_clock::now() - start) >= timeout) {
        return pred(get());
      }
      std::this_thread::sleep_for(kPollInterval);
    }
    return true;
  }

  bool wait_for(StateEnum target, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) const noexcept {
    return wait_until([target](StateEnum s) { return s == target; }, timeout);
  }

  bool wait_for_stable(std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) const noexcept {
    return wait_until([](StateEnum s) { return !IsTransitional || !IsTransitional(s); }, timeout);
  }

 private:
  static constexpr std::chrono::milliseconds kPollInterval{10};
  std::atomic<StateEnum> state_;
};

}  // namespace app::utils