#include "WiFiAdapter.hpp"

#include <algorithm>

#include "logger.hpp"

// todo add extensive error handling and logging

namespace app::network {

WiFiAdapter::WiFiAdapter(void* context, std::string_view hostname) : context_(context), hostname_(hostname) {}

WiFiAdapter::~WiFiAdapter() noexcept { deinit(); }

WiFiAdapter::WiFiAdapter(WiFiAdapter&& other) noexcept {
  // todo
}

WiFiAdapter& WiFiAdapter::operator=(WiFiAdapter&& other) noexcept {
  // todo
  return *this;
}

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

void WiFiAdapter::registerEventHandler(NetworkEvent event, StationDisconnectionCallback cb) {
  if (event != NetworkEvent::StationDisconnected) {
    logger::warn("Attempted to register StationDisconnectionCallback for non-StationDisconnected event");
    return;
  }

  stationDisconnectionCallback_ = cb;
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, AccessPointStartedCallback cb) {
  if (event != NetworkEvent::AccessPointStarted) {
    logger::warn("Attempted to register AccessPointStartedCallback for non-AccessPointStarted event");
    return;
  }

  accessPointStartedCallback_ = cb;
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, AccessPointStoppedCallback cb) {
  if (event != NetworkEvent::AccessPointStopped) {
    logger::warn("Attempted to register AccessPointStoppedCallback for non-AccessPointStopped event");
    return;
  }

  accessPointStoppedCallback_ = cb;
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
        if (stationDisconnectionCallback_ != nullptr) {
          stationDisconnectionCallback_(context_);
        }
        break;
      case WIFI_EVENT_AP_STACONNECTED:
        if (accessPointStartedCallback_ != nullptr) {
          accessPointStartedCallback_(context_);
        }
        break;
      case WIFI_EVENT_AP_STADISCONNECTED:
        if (accessPointStoppedCallback_ != nullptr) {
          accessPointStoppedCallback_(context_);
        }
        break;
      default:
        logger::warn("Received unhandled WIFI_EVENT with id: %d", id);
    }
  } else if (base == IP_EVENT) {
    switch (id) {
      case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t* event_data = static_cast<ip_event_got_ip_t*>(data);
        app::network::IpAddress ip_address{event_data->ip_info.ip.addr};
        if (stationConnectionCallback_ != nullptr) {
          stationConnectionCallback_(context_, ip_address);
        }
        break;
      }
      default:
        logger::warn("Received unhandled IP_EVENT with id: %d", id);
    }
  } else {
    logger::warn("Received event with unknown base: %s", base);
  }
}

}  // namespace app::network