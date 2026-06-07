#include "StorageManager.hpp"

#include "logger.hpp"


// todo consider refactor to nvs transactions and migrating from initialized_ flag

namespace app::storage {

namespace {
constexpr const char* kAddressLineKey = "address_line";
constexpr const char* kCityKey = "city";
constexpr const char* kPostalCodeKey = "postcode";
constexpr const char* kCountryKey = "country";
constexpr const char* kLatitudeKey = "lat";
constexpr const char* kLongitudeKey = "lon";
constexpr const char* kApiTokenKey = "owm_token";
constexpr const char* kSsidKey = "ssid";
constexpr const char* kPasswordKey = "pass";
constexpr const char* kGeofencePendingKey = "geocode_pending";


}  // namespace

void StorageManager::init() {
  if (initialized_) {
    return;
  }

  nvs_.init();

  ssid_ = nvs_.readString(kSsidKey, "");
  password_ = nvs_.readString(kPasswordKey, "");
  api_token_ = nvs_.readString(kApiTokenKey, "");

  address_line_ = nvs_.readString(kAddressLineKey, "");
  city_ = nvs_.readString(kCityKey, "");
  postal_code_ = nvs_.readString(kPostalCodeKey, "");
  country_ = nvs_.readString(kCountryKey, "");

  latitude_ = nvs_.readString(kLatitudeKey, "52.2297");
  longitude_ = nvs_.readString(kLongitudeKey, "21.0122");
  timezone_offset_seconds_ = nvs_.readInt32("timezone_offset", 0);

  initialized_ = true;

  logger::info("Storage initialized. Configuration values loaded to RAM.");
  logger::debug("Loaded values: ssid=\"{}\", pass=\"{}\", token=\"***\"", ssid_.c_str(), password_.c_str());
}

void StorageManager::saveStringIfChanged(const char* key, std::string& current_value, const std::string& new_value) {
  if (current_value != new_value) {
    current_value = new_value;
    nvs_.saveString(key, new_value);
  }
}

void StorageManager::saveInt32IfChanged(const char* key, int32_t& current_value, int32_t new_value) {
  if (current_value != new_value) {
    current_value = new_value;
    nvs_.saveInt32(key, new_value);
  }
}

void StorageManager::saveU8IfChanged(const char* key, uint8_t& current_value, uint8_t new_value) {
  if (current_value != new_value) {
    current_value = new_value;
    nvs_.saveU8(key, new_value);
  }
}

void StorageManager::saveNetworkCredentials(const std::string& new_ssid, const std::string& new_password) {
  saveStringIfChanged(kSsidKey, ssid_, new_ssid);
  saveStringIfChanged(kPasswordKey, password_, new_password);

  nvs_.commit();

  logger::debug("Saved WiFi credentials: ssid=\"{}\", pass=\"{}\"", new_ssid, new_password);
}

void StorageManager::saveLocationAddress(const std::string& new_address_line, const std::string& new_city,
                                         const std::string& new_postal_code, const std::string& new_country) {
  saveStringIfChanged(kAddressLineKey, address_line_, new_address_line);
  saveStringIfChanged(kCityKey, city_, new_city);
  saveStringIfChanged(kPostalCodeKey, postal_code_, new_postal_code);
  saveStringIfChanged(kCountryKey, country_, new_country);

  nvs_.commit();

  logger::debug("Saved location address.");
}

void StorageManager::saveAppSettings(const std::string& new_token, const std::string& new_lat,
                                     const std::string& new_lon) {
  saveStringIfChanged(kApiTokenKey, api_token_, new_token);
  if (!new_lat.empty()) {
    saveStringIfChanged(kLatitudeKey, latitude_, new_lat);
  }
  if (!new_lon.empty()) {
    saveStringIfChanged(kLongitudeKey, longitude_, new_lon);
  }

  nvs_.commit();

  logger::debug("Saved application settings: token=\"***\", lat={}, lon={}", new_lat.c_str(), new_lon.c_str());
}

void StorageManager::saveTimezoneOffsetSeconds(int32_t new_timezone_offset_seconds) {
  saveInt32IfChanged("timezone_offset", timezone_offset_seconds_, new_timezone_offset_seconds);

  nvs_.commit();

  logger::debug("Saved timezone offset: {}", new_timezone_offset_seconds);
}


void StorageManager::setGeocodePending(bool pending) {
  saveU8IfChanged(kGeofencePendingKey, geocodePending_, pending ? 1 : 0);

  nvs_.commit();
}

}  // namespace app::storage