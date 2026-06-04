#pragma once

#include <nvs.h>

#include <string_view>

namespace app::storage {

enum class NVSOpenMode { ReadOnly = NVS_READONLY, ReadWrite = NVS_READWRITE };

class NVSAdapter {
 public:
  NVSAdapter(std::string_view namespace_name);
  ~NVSAdapter() noexcept;
  NVSAdapter(const NVSAdapter&) = delete;
  NVSAdapter& operator=(const NVSAdapter&) = delete;
  NVSAdapter(NVSAdapter&& other) noexcept;
  NVSAdapter& operator=(NVSAdapter&& other) noexcept;


  void open(NVSOpenMode mode = NVSOpenMode::ReadOnly);
  void close();
  void commit();
  const bool init();

  std::string readString(const char* key, const char* defaultValue = "") const;
  int32_t readInt32(const char* key, int32_t defaultValue = 0) const;
  uint8_t readU8(const char* key, uint8_t defaultValue = 0) const;
  void saveU8(const char* key, uint8_t value);
  void saveString(const char* key, const std::string& value);
  void saveInt32(const char* key, int32_t value);


  [[nodiscard]] const bool isOpen() const { return handle_ != 0; }

 private:
  std::string_view namespace_;
  bool is_initialized_ = false;
  nvs_handle_t handle_ = 0;
};

}  // namespace app::storage
