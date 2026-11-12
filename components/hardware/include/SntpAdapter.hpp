#pragma once

#include <cstdint>
#include <ctime>

#include "State.hpp"
#include "TaskRunner.hpp"

namespace app::hardware {

enum class SntpStatus : uint8_t {
  NotInitialized = 0x00,
  Initialized = 0x01,  // SNTP initialized but not synchronized or started
  Synchronized = 0x02,

  InitializationFailed = 0x20,
  SynchronizationFailed = 0x21,

  Initializing = 0x40,
  Synchronizing = 0x41
};

constexpr const char* to_string(SntpStatus status) noexcept {
  switch (status) {
    case SntpStatus::NotInitialized:
      return "NotInitialized";
    case SntpStatus::Initialized:
      return "Initialized";
    case SntpStatus::Synchronized:
      return "Synchronized";
    case SntpStatus::InitializationFailed:
      return "InitializationFailed";
    case SntpStatus::SynchronizationFailed:
      return "SynchronizationFailed";
    case SntpStatus::Initializing:
      return "Initializing";
    case SntpStatus::Synchronizing:
      return "Synchronizing";
  }
  return "Unknown";
}


class SntpAdapter {
 public:
  using SntpSynchronizationCallback = void (*)(void* context, time_t sync_time);

  explicit SntpAdapter(void* context = nullptr) : context_(context) {}
  ~SntpAdapter();

  SntpAdapter(const SntpAdapter&) = delete;
  SntpAdapter& operator=(const SntpAdapter&) = delete;
  SntpAdapter(SntpAdapter&&) = delete;
  SntpAdapter& operator=(SntpAdapter&&) = delete;

  void init();
  void setSyncCallback(SntpSynchronizationCallback callback);
  void start();

  [[nodiscard]] bool isInitialized() const;
  [[nodiscard]] SntpStatus getStatus() const;
  [[nodiscard]] bool isSynchronized() const;

 private:
  static void taskEntry(void* context);
  void runSyncTask();

  void* context_{nullptr};
  SntpSynchronizationCallback sync_callback_{nullptr};
  utils::State<SntpStatus> sync_status_{SntpStatus::NotInitialized};
  bool sntp_initialized_{false};
  utils::TaskRunner sync_task_;
};

}  // namespace app::hardware
