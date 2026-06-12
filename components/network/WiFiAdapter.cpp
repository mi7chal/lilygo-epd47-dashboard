#include "WiFiAdapter.hpp"

#include <mdns.h>

#include <algorithm>

#include "logger.hpp"

// todo add extensive error handling and logging

namespace app::network {

WiFiAdapter::WiFiAdapter(void* context, NetworkState& network_state, std::string_view hostname)
    : context_(context), network_state_(network_state), hostname_(hostname) {}

WiFiAdapter::~WiFiAdapter() noexcept { deinit(); }



bool WiFiAdapter::init() {
  if (initialized_) {
    return true;
  }

  wifi_event_group_ = xEventGroupCreate();
  if (wifi_event_group_ == nullptr) {
    logger::error("Failed to create WiFi event group");
    return false;
  }

  esp_err_t err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiAdapter::esp_event_dispatcher,
                                                      this, &wifi_event_instance);

  if (err != ESP_OK) {
    logger::error("Failed to register WIFI_EVENT handler: {}", esp_err_to_name(err));
    return false;
  }

  err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiAdapter::esp_event_dispatcher, this,
                                            &ip_event_instance);

  if (err != ESP_OK) {
    logger::error("Failed to register IP_EVENT handler: {}", esp_err_to_name(err));
    return false;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_wifi_set_storage(WIFI_STORAGE_RAM);  // config unknown at compile-time is stored in StorageManager

  initialized_ = true;
  return true;
}

void WiFiAdapter::deinit() {
  esp_wifi_stop();

  esp_wifi_deinit();

  if (sta_netif != nullptr) {
    esp_netif_destroy(sta_netif);
    sta_netif = nullptr;
  }
  if (ap_netif != nullptr) {
    esp_netif_destroy(ap_netif);
    ap_netif = nullptr;
  }

  esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_instance);
  esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_instance);
}

std::optional<WiFiConnectionInfo> WiFiAdapter::getWiFiConnectionInfo() const {
  if (!initialized_ || !network_state_.isStaConnected()) {
    return std::nullopt;
  }

  wifi_ap_record_t apRecord{};
  if (esp_wifi_sta_get_ap_info(&apRecord) != ESP_OK) {
    logger::warn("Failed to read STA info for connection info");
    return std::nullopt;
  }

  esp_netif_ip_info_t ipInfo{};
  if (esp_netif_get_ip_info(sta_netif, &ipInfo) != ESP_OK) {
    logger::warn("Failed to read IP info for connection info");
    return std::nullopt;
  }

  // technically hostname could be gotten from netif too, but it's already stored
  return WiFiConnectionInfo{std::string(reinterpret_cast<const char*>(apRecord.ssid)), std::string{hostname_},
                            IpAddress{ipInfo.ip.addr}, apRecord.rssi};
}

std::optional<AccessPointInfo> WiFiAdapter::getAccessPointInfo() const {
  if (!initialized_ || !network_state_.isApActive()) {
    return std::nullopt;
  }

  esp_netif_ip_info_t ipInfo{};
  if (esp_netif_get_ip_info(ap_netif, &ipInfo) != ESP_OK) {
    logger::warn("Failed to read IP info for access point info");
    return std::nullopt;
  }

  wifi_config_t ap_config{};
  if (esp_wifi_get_config(WIFI_IF_AP, &ap_config) != ESP_OK) {
    logger::warn("Failed to read AP config for access point info");
    return std::nullopt;
  }

  // technically hostname could be gotten from netif too, but it's already stored
  return AccessPointInfo{std::string(reinterpret_cast<const char*>(ap_config.ap.ssid)),
                         std::string(reinterpret_cast<const char*>(ap_config.ap.password)), std::string{hostname_},
                         IpAddress{ipInfo.ip.addr}};
}


void WiFiAdapter::initAccessPoint() {
  if (!initialized_) {
    logger::warn("Attempted to initialize Access Point without general initialization");
    return;
  }

  // it basically meants that ap is already initialized
  if (ap_netif != nullptr) {
    return;
  }

  ap_netif = esp_netif_create_default_wifi_ap();

  if (ap_netif == nullptr) {
    logger::error("Failed to create default WiFi AP network interface");
    return;
  }

  // --- DHCP 114 ---
  // https://datatracker.ietf.org/doc/rfc8910/
  esp_err_t err = esp_netif_dhcps_stop(ap_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    logger::warn("Failed to stop DHCP server for option configuration: {}", esp_err_to_name(err));
  }

  constexpr const char* kPortalUri = "http://192.168.4.1/";  // todo move to constant or more appropriate place
  const std::uint8_t uri_len = static_cast<std::uint8_t>(std::strlen(kPortalUri));

  err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, const_cast<char*>(kPortalUri),
                               uri_len);
  if (err != ESP_OK) {
    logger::warn("Failed to set DHCP Option 114: {}", esp_err_to_name(err));
  }

  err = esp_netif_dhcps_start(ap_netif);
  if (err != ESP_OK) {
    logger::error("Failed to restart DHCP server: {}", esp_err_to_name(err));
  }
}

void WiFiAdapter::initStation() {
  if (!initialized_) {
    logger::warn("Attempted to initialize Station without general initialization");
    return;
  }

  // it basically meants that station is already initialized
  if (sta_netif != nullptr) {
    return;
  }

  sta_netif = esp_netif_create_default_wifi_sta();
}

void WiFiAdapter::enableMDNS(std::string_view instance_name) {
  const esp_err_t mdnsErr = mdns_init();

  if (mdnsErr == ESP_OK || mdnsErr == ESP_ERR_INVALID_STATE) {
    mdns_hostname_set(hostname_.data());

    if (!instance_name.empty()) {
      mdns_instance_name_set(instance_name.data());
    }

    logger::info("mDNS responder started. Access device under {}.local", hostname_);
  } else {
    logger::error("Error setting up mDNS responder: {}", esp_err_to_name(mdnsErr));
  }
}

void WiFiAdapter::startAccessPoint(std::string_view ssid, std::string_view password = "") {
  if (!initialized_) {
    logger::warn("Attempted to start Access Point without initialization");
    return;
  }

  initAccessPoint();  // it will do nothing if AP is already initialized

  esp_wifi_stop();  // ensure WiFi is stopped before changing mode

  wifi_config_t ap_config{};

  std::copy_n(ssid.begin(), std::min<std::size_t>(ssid.size(), sizeof(ap_config.ap.ssid)), ap_config.ap.ssid);
  ap_config.ap.ssid_len = static_cast<uint8_t>(std::min<std::size_t>(ssid.size(), sizeof(ap_config.ap.ssid)));

  if (password.empty()) {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  } else {
    std::copy_n(password.begin(), std::min<std::size_t>(password.size(), sizeof(ap_config.ap.password)),
                ap_config.ap.password);

    ap_config.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;  // mixed mode for compatibility
  }

  ap_config.ap.max_connection = 4;
  ap_config.ap.channel = 1;
  ap_config.ap.ssid_hidden = 0;

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);

  esp_wifi_start();
}

void WiFiAdapter::stopAccessPoint() {
  if (!initialized_) {
    logger::warn("Attempted to stop Access Point without initialization");
    return;
  }

  esp_wifi_stop();
}

void WiFiAdapter::connectToWiFi(std::string_view ssid, std::string_view password) {
  if (!initialized_) {
    logger::warn("Attempted to connect without initialization");
    return;
  }

  initStation();  // it will do nothing if sta is already initialized

  esp_wifi_stop();  // ensure WiFi is stopped before changing mode

  wifi_config_t sta_config{};

  sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  sta_config.sta.pmf_cfg.capable = true;
  sta_config.sta.pmf_cfg.required = false;

  std::copy_n(ssid.begin(), std::min<std::size_t>(ssid.size(), sizeof(sta_config.sta.ssid)), sta_config.sta.ssid);
  std::copy_n(password.begin(), std::min<std::size_t>(password.size(), sizeof(sta_config.sta.password)),
              sta_config.sta.password);

  esp_wifi_set_config(WIFI_IF_STA, &sta_config);

  esp_wifi_set_mode(WIFI_MODE_STA);

  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // powe save mode

  esp_netif_set_hostname(sta_netif, hostname_.data());

  esp_wifi_start();

  esp_wifi_connect();
}

void WiFiAdapter::disconnectFromWiFi() {
  if (!initialized_) {
    logger::warn("Attempted to disconnect without initialization");
    return;
  }

  esp_wifi_stop();

  logger::debug("Disconnected from WiFi");
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, StationConnectionCallback cb) {
  if (event != NetworkEvent::StationConnectionEstablished) {
    logger::warn(
        "Attempted to register StationConnectionCallback for non-StationConnectionEstablished "
        "event");
    return;
  }

  stationConnectionCallback_ = cb;
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, SimpleEventCallback cb) {
  switch (event) {
    case NetworkEvent::StationDisconnected:
      stationDisconnectionCallback_ = cb;
      break;
    case NetworkEvent::AccessPointStarted:
      accessPointStartedCallback_ = cb;
      break;
    case NetworkEvent::AccessPointStopped:
      accessPointStoppedCallback_ = cb;
      break;
    default:
      logger::warn("Attempted to register SimpleEventCallback for unsupported event");
      break;
  }
}

void WiFiAdapter::unregisterEventHandler(NetworkEvent event) {
  switch (event) {
    case NetworkEvent::StationConnectionEstablished:
      stationConnectionCallback_ = nullptr;
      break;
    case NetworkEvent::StationDisconnected:
      stationDisconnectionCallback_ = nullptr;
      break;
    case NetworkEvent::AccessPointStarted:
      accessPointStartedCallback_ = nullptr;
      break;
    case NetworkEvent::AccessPointStopped:
      accessPointStoppedCallback_ = nullptr;
      break;
    default:
      logger::warn("Attempted to unregister handler for unknown event");
  }
}

void WiFiAdapter::esp_event_dispatcher(void* arg, const char* base, int32_t id, void* data) {
  if (arg != nullptr) {
    auto* adapter = static_cast<WiFiAdapter*>(arg);
    adapter->handle_system_event(base, id, data);
  }
}

void WiFiAdapter::handle_system_event(const char* base, int32_t id, void* data) {
  if (base == WIFI_EVENT) {
    switch (id) {
      case WIFI_EVENT_STA_START:
        esp_wifi_connect();  // wifi should be connected as soon as it starts in station mode
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        network_state_.setStaDisconnected();

        if (stationDisconnectionCallback_ != nullptr) {
          stationDisconnectionCallback_(context_);
        }
        break;
      case WIFI_EVENT_AP_START:
        network_state_.setApConnected();

        if (accessPointStartedCallback_ != nullptr) {
          accessPointStartedCallback_(context_);
        }
        break;
      case WIFI_EVENT_AP_STOP:
        network_state_.setApDisconnected();

        if (accessPointStoppedCallback_ != nullptr) {
          accessPointStoppedCallback_(context_);
        }
        break;
      default:
        logger::warn("Received unhandled WIFI_EVENT with id: {}", id);
    }
  } else if (base == IP_EVENT) {
    switch (id) {
      case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t* event_data = static_cast<ip_event_got_ip_t*>(data);
        app::network::IpAddress ip_address{event_data->ip_info.ip.addr};

        network_state_.setStaConnected();

        if (stationConnectionCallback_ != nullptr) {
          stationConnectionCallback_(context_, ip_address);
        }
        break;
      }
      default:
        logger::warn("Received unhandled IP_EVENT with id: {}", id);
    }
  } else {
    logger::warn("Received event with unknown base: {}", base);
  }
}

}  // namespace app::network