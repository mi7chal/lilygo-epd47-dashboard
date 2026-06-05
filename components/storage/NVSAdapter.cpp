#include "NVSAdapter.hpp"

#include <nvs_flash.h>

#include "logger.hpp"

namespace app::storage {

NVSAdapter::NVSAdapter(std::string_view namespace_name) : namespace_(namespace_name) {}

NVSAdapter::~NVSAdapter() noexcept {
  close();  // close is idempotent, so it's safe to call even if the namespace was never opened
}

NVSAdapter::NVSAdapter(NVSAdapter&& other) noexcept
    : namespace_(other.namespace_), is_initialized_(other.is_initialized_), handle_(other.handle_) {
  other.is_initialized_ = false;
  other.handle_ = 0;
}

NVSAdapter& NVSAdapter::operator=(NVSAdapter&& other) noexcept {
  if (this != &other) {
    if (isOpen()) {
      nvs_close(handle_);  // closing previous
    }

    handle_ = other.handle_;
    is_initialized_ = other.is_initialized_;

    other.is_initialized_ = false;
    other.handle_ = 0;
  }
  return *this;
}


bool NVSAdapter::init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  is_initialized_ = (err == ESP_OK);

  return is_initialized_;
}

void NVSAdapter::open(NVSOpenMode mode = NVSOpenMode::ReadOnly) {
  if (is_initialized_) {
    return;
  }

  esp_err_t err = nvs_open(namespace_.data(), static_cast<nvs_open_mode_t>(mode), &handle_);
  if (err != ESP_OK) {
    logger::error("Failed to open NVS handle: {}", esp_err_to_name(err));
    return;
  }

  is_initialized_ = true;
}

void NVSAdapter::close() {
  if (!is_initialized_) {
    return;
  }

  if (!isOpen()) {
    logger::warn("Attempted to close NVS handle that is not open");
    return;
  }

  nvs_close(handle_);
}

void NVSAdapter::commit() {
  if (!is_initialized_) {
    logger::warn("Attempted to commit NVS changes without initialization");
    return;
  }

  if (!isOpen()) {
    logger::warn("Attempted to commit NVS changes without an open handle");
    return;
  }

  esp_err_t err = nvs_commit(handle_);
  if (err != ESP_OK) {
    logger::error("Failed to commit NVS changes: {}", esp_err_to_name(err));
  }
}

std::string NVSAdapter::readString(const char* key, const char* defaultValue) const {
  if (!is_initialized_) {
    logger::warn("Attempted to read from NVS without initialization");
    return std::string(defaultValue);
  }

  if (!isOpen()) {
    logger::warn("Attempted to read from NVS without an open handle");
    return std::string(defaultValue);
  }

  size_t requiredSize = 0;
  esp_err_t err = nvs_get_str(handle_, key, nullptr, &requiredSize);
  if (err != ESP_OK || requiredSize == 0) {
    return std::string(defaultValue);  // todo consider string_view
  }

  std::string value(requiredSize, '\0');
  err = nvs_get_str(handle_, key, value.data(), &requiredSize);
  if (err != ESP_OK) {
    return std::string(defaultValue);
  }

  if (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }

  return value;
}

int32_t NVSAdapter::readInt32(const char* key, int32_t defaultValue) const {
  if (!is_initialized_) {
    logger::warn("Attempted to read from NVS without initialization");
    return defaultValue;
  }

  if (!isOpen()) {
    logger::warn("Attempted to read from NVS without an open handle");
    return defaultValue;
  }

  int32_t value = defaultValue;
  if (nvs_get_i32(handle_, key, &value) != ESP_OK) {
    return defaultValue;
  }

  return value;
}

uint8_t NVSAdapter::readU8(const char* key, uint8_t defaultValue) const {
  if (!is_initialized_) {
    logger::warn("Attempted to read from NVS without initialization");
    return defaultValue;
  }

  if (!isOpen()) {
    logger::warn("Attempted to read from NVS without an open handle");
    return defaultValue;
  }

  uint8_t value = defaultValue;
  if (nvs_get_u8(handle_, key, &value) != ESP_OK) {
    return defaultValue;
  }

  return value;
}

void NVSAdapter::saveU8(const char* key, uint8_t value) {
  if (!is_initialized_) {
    logger::warn("Attempted to write to NVS without initialization");
    return;
  }

  if (!isOpen()) {
    logger::warn("Attempted to write to NVS without an open handle");
    return;
  }

  esp_err_t err = nvs_set_u8(handle_, key, value);
  if (err != ESP_OK) {
    logger::error("Failed to save uint8_t to NVS: {}", esp_err_to_name(err));
  }
}

void NVSAdapter::saveString(const char* key, const std::string& value) {
  if (!is_initialized_) {
    logger::warn("Attempted to write to NVS without initialization");
    return;
  }

  if (!isOpen()) {
    logger::warn("Attempted to write to NVS without an open handle");
    return;
  }

  esp_err_t err = nvs_set_str(handle_, key, value.c_str());
  if (err != ESP_OK) {
    logger::error("Failed to save string to NVS: {}", esp_err_to_name(err));
  }
}

void NVSAdapter::saveInt32(const char* key, int32_t value) {
  if (!is_initialized_) {
    logger::warn("Attempted to write to NVS without initialization");
    return;
  }

  if (!isOpen()) {
    logger::warn("Attempted to write to NVS without an open handle");
    return;
  }

  esp_err_t err = nvs_set_i32(handle_, key, value);
  if (err != ESP_OK) {
    logger::error("Failed to save int32_t to NVS: {}", esp_err_to_name(err));
  }
}