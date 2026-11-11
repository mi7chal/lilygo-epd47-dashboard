#include "SntpAdapter.hpp"

#include <esp_netif_sntp.h>
#include <esp_sntp.h>

#include <atomic>

#include "logger.hpp"

namespace app::hardware {

namespace {
std::atomic<bool> g_adapter_exists{false};

constexpr TickType_t kWaitTicks = pdMS_TO_TICKS(500);
}  // namespace

SntpAdapter::~SntpAdapter() {
  sync_task_.stop();
  if (!sntp_initialized_) return;

  esp_netif_sntp_deinit();
  g_adapter_exists = false;
}

void SntpAdapter::init() {
  if (!sync_status_.transition(SntpStatus::NotInitialized, SntpStatus::Initializing) &&
      !sync_status_.transition(SntpStatus::InitializationFailed, SntpStatus::Initializing)) {
    logger::warn("SNTP cannot be initialized, current status: {}", to_string(sync_status_.get()));
    return;
  }

  if (g_adapter_exists.exchange(true)) {
    logger::error("Another SntpAdapter is already initialized");
    sync_status_.set(SntpStatus::InitializationFailed);
    return;
  }

  if (esp_sntp_enabled()) {
    logger::warn("SNTP already enabled, deinitializing first to ensure clean state");
    esp_netif_sntp_deinit();
  }

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  config.server_from_dhcp = true;
  config.renew_servers_after_new_IP = true;
  config.index_of_first_server = 1;
  config.start = false;

  const esp_err_t err = esp_netif_sntp_init(&config);
  if (err != ESP_OK) {
    logger::error("Failed to init SNTP: {}", esp_err_to_name(err));
    sync_status_.set(SntpStatus::InitializationFailed);
    g_adapter_exists = false;
    return;
  }

  sntp_initialized_ = true;
  sync_status_.set(SntpStatus::Initialized);
  logger::info("SNTP adapter initialized successfully");
}

void SntpAdapter::setSyncCallback(SntpSynchronizationCallback callback) { sync_callback_ = callback; }

void SntpAdapter::start() {
  if (!sync_status_.transition(SntpStatus::Initialized, SntpStatus::Synchronizing) &&
      !sync_status_.transition(SntpStatus::SynchronizationFailed, SntpStatus::Synchronizing)) {
    logger::warn("Cannot start SNTP, current state: {}", to_string(sync_status_.get()));
    return;
  }

  const esp_err_t err = esp_netif_sntp_start();
  if (err != ESP_OK) {
    logger::error("Failed to start SNTP: {}", esp_err_to_name(err));
    sync_status_.set(SntpStatus::SynchronizationFailed);
    return;
  }

  if (!sync_task_.start(&SntpAdapter::taskEntry, this, "sntp_sync_task", 3072, 5)) {
    logger::error("Failed to create SNTP sync task");
    sync_status_.set(SntpStatus::SynchronizationFailed);
    return;
  }

  logger::info("SNTP synchronization started");
}

bool SntpAdapter::isInitialized() const {
  const SntpStatus current = sync_status_.get();

  return current == SntpStatus::Initialized || current == SntpStatus::Synchronizing ||
         current == SntpStatus::Synchronized;
}

SntpStatus SntpAdapter::getStatus() const { return sync_status_.get(); }

bool SntpAdapter::isSynchronized() const { return sync_status_.is(SntpStatus::Synchronized); }

void SntpAdapter::taskEntry(void* context) { static_cast<SntpAdapter*>(context)->runSyncTask(); }

// No overall timeout: SNTP keeps retrying, so a slow network just syncs later.
void SntpAdapter::runSyncTask() {
  esp_err_t err = ESP_ERR_TIMEOUT;
  while (!sync_task_.stopRequested() && (err = esp_netif_sntp_sync_wait(kWaitTicks)) != ESP_OK) {
    if (err != ESP_ERR_TIMEOUT && err != ESP_ERR_NOT_FINISHED) {
      logger::warn("SNTP synchronization failed: {}", esp_err_to_name(err));
      sync_status_.set(SntpStatus::SynchronizationFailed);
      return;
    }
  }
  if (err != ESP_OK) return;  // stopped before the first sync

  time_t now = 0;
  time(&now);
  logger::info("Time synchronized successfully, epoch: {}", now);
  sync_status_.set(SntpStatus::Synchronized);
  if (sync_callback_ != nullptr) {
    sync_callback_(context_, now);
  }
}

}  // namespace app::hardware
