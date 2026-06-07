#pragma once

#include <cstdint>
#include <string>

#include "NVSAdapter.hpp"

namespace app::storage {

class StorageManager {
 public:
  StorageManager() = default;

  void init();

  void saveNetworkCredentials(const std::string& new_ssid, const std::string& new_password);
  void saveLocationAddress(const std::string& new_address_line, const std::string& new_city,
                           const std::string& new_postal_code, const std::string& new_country);
  void saveAppSettings(const std::string& new_token, const std::string& new_latitude, const std::string& new_longitude);
  void saveTimezoneOffsetSeconds(int32_t new_timezone_offset_seconds);

  [[nodiscard]] const std::string& getSSID() const { return ssid_; }
  [[nodiscard]] const std::string& getPassword() const { return password_; }
  [[nodiscard]] const std::string& getApiToken() const { return api_token_; }
  [[nodiscard]] const std::string& getAddressLine() const { return address_line_; }
  [[nodiscard]] const std::string& getCity() const { return city_; }
  [[nodiscard]] const std::string& getPostalCode() const { return postal_code_; }
  [[nodiscard]] const std::string& getCountry() const { return country_; }
  [[nodiscard]] const std::string& getLatitude() const { return latitude_; }
  [[nodiscard]] const std::string& getLongitude() const { return longitude_; }
  [[nodiscard]] int32_t getTimezoneOffsetSeconds() const { return timezone_offset_seconds_; }
  [[nodiscard]] bool isGeocodePending() const { return geocodePending_ != 0; };

  void setGeocodePending(bool pending);

 private:
  static constexpr const char* NVS_NAMESPACE = "lilygo_dash";
  NVSAdapter nvs_{NVS_NAMESPACE};

  bool initialized_ = false;

  std::string ssid_;
  std::string password_;
  std::string api_token_;
  std::string address_line_;
  std::string city_;
  std::string postal_code_;
  std::string country_;
  std::string latitude_;
  std::string longitude_;

  int32_t timezone_offset_seconds_ = 0;
  uint8_t geocodePending_ = 0;

  void saveStringIfChanged(const char* key, std::string& currentValue, const std::string& newValue);
  void saveInt32IfChanged(const char* key, int32_t& currentValue, int32_t newValue);
  void saveU8IfChanged(const char* key, uint8_t& currentValue, uint8_t newValue);
};
}  // namespace app::storage