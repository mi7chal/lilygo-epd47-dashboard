#include "NetworkManager.h"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/inet.h>
#include <mdns.h>
#include <nvs_flash.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include "logger.hpp"

namespace {
constexpr const char* HOSTNAME = "lilygo-dashboard";
constexpr const char* WIFI_PORTAL_NAME = "LilyGO Dashboard";
constexpr const char* WIFI_PORTAL_PASSWORD = "lilygo678";
constexpr std::uint32_t WIFI_CONNECTION_TIMEOUT_SECONDS = 30U;
constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
constexpr EventBits_t WIFI_FAIL_BIT = BIT1;

EventGroupHandle_t wifiEventGroup = nullptr;
esp_netif_t* staNetif = nullptr;
esp_netif_t* apNetif = nullptr;
bool wifiInitialised = false;
bool handlersRegistered = false;
esp_event_handler_instance_t wifiHandlerInstance{};
esp_event_handler_instance_t ipHandlerInstance{};

std::string trim_copy(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
    return std::isspace(character) != 0;
  });

  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
                      return std::isspace(character) != 0;
                    }).base();

  if (first >= last) {
    return {};
  }

  return std::string(first, last);
}

std::string ipToString(const esp_ip4_addr_t& ip) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&ip));
  return buffer;
}


bool ensureWifiStackInitialised() {
  if (wifiInitialised) {
    return true;
  }


  const esp_err_t netifErr = esp_netif_init();
  if (netifErr != ESP_OK && netifErr != ESP_ERR_INVALID_STATE) {
    app::logger::error("esp_netif_init failed: {}", esp_err_to_name(netifErr));
    return false;
  }

  const esp_err_t eventLoopErr = esp_event_loop_create_default();
  if (eventLoopErr != ESP_OK && eventLoopErr != ESP_ERR_INVALID_STATE) {
    app::logger::error("esp_event_loop_create_default failed: {}", esp_err_to_name(eventLoopErr));
    return false;
  }

  if (staNetif == nullptr) {
    staNetif = esp_netif_create_default_wifi_sta();
  }
  if (apNetif == nullptr) {
    apNetif = esp_netif_create_default_wifi_ap();
  }

  wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();
  const esp_err_t wifiInitErr = esp_wifi_init(&wifiConfig);
  if (wifiInitErr != ESP_OK && wifiInitErr != ESP_ERR_WIFI_INIT_STATE) {
    app::logger::error("esp_wifi_init failed: {}", esp_err_to_name(wifiInitErr));
    return false;
  }

  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  if (!handlersRegistered) {
    const esp_err_t wifiHandlerErr = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void*, esp_event_base_t eventBase, int32_t eventId, void*) {
          if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
          } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
            if (wifiEventGroup != nullptr) {
              xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);
            }
          }
        },
        nullptr, &wifiHandlerInstance);

    const esp_err_t ipHandlerErr = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](void*, esp_event_base_t, int32_t, void*) {
          if (wifiEventGroup != nullptr) {
            xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
          }
        },
        nullptr, &ipHandlerInstance);

    if (wifiHandlerErr != ESP_OK || ipHandlerErr != ESP_OK) {
      app::logger::error("failed to register Wi-Fi event handlers");
      return false;
    }

    handlersRegistered = true;
  }

  if (wifiEventGroup == nullptr) {
    wifiEventGroup = xEventGroupCreate();
  }

  wifiInitialised = true;
  return true;
}
}  // namespace

namespace app::network {
AccessPointInfo NetworkManager::enableAccessPoint() {
  app::logger::info("enabling WiFi Captive portal (Access Point) for configuration");

  if (!ensureWifiStackInitialised()) {
    return {};
  }

  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_AP);

  wifi_config_t wifiConfig{};
  std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.ssid), WIFI_PORTAL_NAME,
               sizeof(wifiConfig.ap.ssid));
  wifiConfig.ap.ssid_len = static_cast<uint8_t>(std::strlen(WIFI_PORTAL_NAME));
  std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.password), WIFI_PORTAL_PASSWORD,
               sizeof(wifiConfig.ap.password));
  wifiConfig.ap.channel = 1;
  wifiConfig.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  wifiConfig.ap.max_connection = 4;
  wifiConfig.ap.beacon_interval = 100;

  esp_wifi_set_config(WIFI_IF_AP, &wifiConfig);

  esp_netif_ip_info_t ipInfo{};
  IP4_ADDR(&ipInfo.ip, 192, 168, 4, 1);
  IP4_ADDR(&ipInfo.gw, 192, 168, 4, 1);
  IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);

  if (apNetif != nullptr) {
    esp_netif_dhcps_stop(apNetif);
    esp_netif_set_ip_info(apNetif, &ipInfo);
    esp_netif_dhcps_start(apNetif);
    esp_netif_set_hostname(apNetif, HOSTNAME);
  }

  esp_wifi_start();

  const esp_err_t mdnsErr = mdns_init();
  if (mdnsErr == ESP_OK || mdnsErr == ESP_ERR_INVALID_STATE) {
    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set("LilyGO Dashboard");
    app::logger::info("mDNS responder started. Access device under {}.local", HOSTNAME);
  } else {
    app::logger::error("Error setting up mDNS responder: {}", esp_err_to_name(mdnsErr));
  }

  app::logger::info("WiFi Captive Portal (Access Point) enabled with SSID: {} and password: ***",
                    WIFI_PORTAL_NAME);

  return AccessPointInfo{WIFI_PORTAL_NAME, WIFI_PORTAL_PASSWORD, HOSTNAME, ipToString(ipInfo.ip)};
}

std::optional<WiFiConnectionInfo> NetworkManager::connectToWiFi(const std::string& ssid,
                                                                const std::string& password) {
  const std::string trimmedSsid = trim_copy(ssid);
  const std::string trimmedPassword = trim_copy(password);

  if (trimmedSsid.empty()) {
    app::logger::warn("no WiFi credentials stored");
    return std::nullopt;
  }

  app::logger::info("connecting to WiFi SSID={}", trimmedSsid.c_str());

  if (!ensureWifiStackInitialised()) {
    return std::nullopt;
  }

  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_STA);

  if (staNetif != nullptr) {
    esp_netif_set_hostname(staNetif, HOSTNAME);
  }

  wifi_config_t wifiConfig{};
  std::strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), trimmedSsid.c_str(),
               sizeof(wifiConfig.sta.ssid));
  std::strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), trimmedPassword.c_str(),
               sizeof(wifiConfig.sta.password));
  wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifiConfig.sta.pmf_cfg.capable = true;
  wifiConfig.sta.pmf_cfg.required = false;

  esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);

  if (wifiEventGroup != nullptr) {
    xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
  }

  esp_wifi_start();
  esp_wifi_connect();

  if (wifiEventGroup == nullptr) {
    return std::nullopt;
  }

  const EventBits_t bits =
      xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                          pdMS_TO_TICKS(WIFI_CONNECTION_TIMEOUT_SECONDS * 1000U));

  if ((bits & WIFI_CONNECTED_BIT) == 0) {
    if (bits & WIFI_FAIL_BIT) {
      app::logger::warn("WiFi connection failed");
    } else {
      app::logger::warn("WiFi connection timed out");
    }
    return std::nullopt;
  }

  esp_netif_ip_info_t ipInfo{};
  if (staNetif == nullptr || esp_netif_get_ip_info(staNetif, &ipInfo) != ESP_OK) {
    app::logger::error("Failed to read STA IP information");
    return std::nullopt;
  }

  wifi_ap_record_t apRecord{};
  int32_t rssi = 0;
  if (esp_wifi_sta_get_ap_info(&apRecord) == ESP_OK) {
    rssi = apRecord.rssi;
  } else {
    app::logger::warn("Failed to read RSSI from current AP");
  }

  app::logger::info("WiFi connected, IP address: {}", ipToString(ipInfo.ip).c_str());

  const esp_err_t mdnsErr = mdns_init();
  if (mdnsErr == ESP_OK || mdnsErr == ESP_ERR_INVALID_STATE) {
    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set("LilyGO Dashboard");
    app::logger::info("mDNS responder started. Access device under {}.local", HOSTNAME);
  } else {
    app::logger::error("Error setting up mDNS responder: {}", esp_err_to_name(mdnsErr));
  }

  return std::make_optional(WiFiConnectionInfo{trimmedSsid, HOSTNAME, ipToString(ipInfo.ip), rssi});
}

}  // namespace app::network