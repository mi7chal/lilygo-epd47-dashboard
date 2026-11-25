#include "WiFiAdapter.hpp"

#include <mdns.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "logger.hpp"

// todo add extensive error handling and logging

namespace app::network {

WiFiAdapter::WiFiAdapter(void* context, NetworkState& network_state, std::string hostname)
    : context_(context), network_state_(network_state), hostname_(std::move(hostname)) {}

WiFiAdapter::~WiFiAdapter() noexcept { deinit(); }


bool WiFiAdapter::init() {
  if (initialized_) {
    return true;
  }

  esp_err_t err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiAdapter::espDispatchEvent,
                                                      this, &wifi_event_delegating_handler_);

  if (err != ESP_OK) {
    logger::error("Failed to register WIFI_EVENT handler: {}", esp_err_to_name(err));
    return false;
  }

  err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &WiFiAdapter::espDispatchEvent, this,
                                            &ip_event_delegating_handler_);

  if (err != ESP_OK) {
    logger::error("Failed to register IP_EVENT handler: {}", esp_err_to_name(err));
    unregisterEspHandlers();
    return false;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    logger::error("Failed to init WiFi: {}", esp_err_to_name(err));
    unregisterEspHandlers();
    return false;
  }

  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);  // config unknown at compile-time is stored in StorageManager
  if (err != ESP_OK) {
    logger::warn("Failed to set WiFi storage to RAM: {}", esp_err_to_name(err));
  }
  initialized_ = true;
  return true;
}

void WiFiAdapter::unregisterEspHandlers() {
  if (wifi_event_delegating_handler_ != nullptr) {
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_delegating_handler_);
    wifi_event_delegating_handler_ = nullptr;
  }
  if (ip_event_delegating_handler_ != nullptr) {
    esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_delegating_handler_);
    ip_event_delegating_handler_ = nullptr;
  }
}

void WiFiAdapter::deinit() {
  if (!initialized_) {
    return;
  }

  sta_started_.store(false, std::memory_order_release);

  // handlers first: no callback may reach the owner during teardown
  unregisterEspHandlers();

  esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
    logger::warn("Failed to stop WiFi during deinit: {}", esp_err_to_name(err));
  }

  err = esp_wifi_deinit();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
    logger::warn("Failed to deinit WiFi: {}", esp_err_to_name(err));
  }

  if (mdns_initialized_) {
    mdns_free();
    mdns_initialized_ = false;
  }

  // plain esp_netif_destroy would leave the WiFi glue dangling
  if (sta_netif != nullptr) {
    esp_netif_destroy_default_wifi(sta_netif);
    sta_netif = nullptr;
  }
  if (ap_netif != nullptr) {
    esp_netif_destroy_default_wifi(ap_netif);
    ap_netif = nullptr;
  }

  network_state_.reset();
  initialized_ = false;
}

std::optional<WiFiConnectionInfo> WiFiAdapter::getWiFiConnectionInfo() const {
  if (!initialized_ || !network_state_.isStaConnected()) {
    return std::nullopt;
  }

  wifi_ap_record_t ap_record{};
  if (esp_wifi_sta_get_ap_info(&ap_record) != ESP_OK) {
    logger::warn("Failed to read STA info for connection info");
    return std::nullopt;
  }

  esp_netif_ip_info_t ip_info{};
  if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK) {
    logger::warn("Failed to read IP info for connection info");
    return std::nullopt;
  }

  const auto* ssid_ptr = reinterpret_cast<const char*>(ap_record.ssid);
  const std::size_t ssid_len = strnlen(ssid_ptr, sizeof(ap_record.ssid));
  return WiFiConnectionInfo{std::string(ssid_ptr, ssid_len), std::string{hostname_}, IpAddress{ip_info.ip.addr},
                            ap_record.rssi};
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

  // ssid is not null-terminated when it uses all 32 bytes; password is not null-terminated when 64 bytes
  const std::size_t ssid_len = std::min<std::size_t>(ap_config.ap.ssid_len, sizeof(ap_config.ap.ssid));
  const auto* pwd_ptr = reinterpret_cast<const char*>(ap_config.ap.password);
  const std::size_t pwd_len = strnlen(pwd_ptr, sizeof(ap_config.ap.password));
  return AccessPointInfo{std::string(reinterpret_cast<const char*>(ap_config.ap.ssid), ssid_len),
                         std::string(pwd_ptr, pwd_len), std::string{hostname_}, IpAddress{ipInfo.ip.addr}};
}


bool WiFiAdapter::initAccessPoint() {
  if (!initialized_) {
    logger::warn("Attempted to initialize Access Point without general initialization");
    return false;
  }

  // it basically means that ap is already initialized
  if (ap_netif != nullptr) {
    return true;
  }

  ap_netif = esp_netif_create_default_wifi_ap();

  if (ap_netif == nullptr) {
    logger::error("Failed to create default WiFi AP network interface");
    return false;
  }

  // --- DHCP 114 ---
  // https://datatracker.ietf.org/doc/rfc8910/
  // Newly created ap_netif starts in ESP_NETIF_DHCP_INIT, so option 114 can be set directly;
  // WIFI_EVENT_AP_START will start the DHCP server once the interface is up.
  esp_netif_ip_info_t ip_info{};
  if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK) {
    logger::warn("Failed to read AP IP, DHCP Option 114 not set");
  } else {
    portal_uri_ = fmt::format("http://{}/", IpAddress{ip_info.ip.addr});

    const esp_err_t err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                                 portal_uri_.data(), static_cast<std::uint32_t>(portal_uri_.size()));
    if (err != ESP_OK) {
      logger::warn("Failed to set DHCP Option 114: {}", esp_err_to_name(err));
    }
  }

  return true;
}

bool WiFiAdapter::initStation() {
  if (!initialized_) {
    logger::warn("Attempted to initialize Station without general initialization");
    return false;
  }

  // it basically means that station is already initialized
  if (sta_netif != nullptr) {
    return true;
  }

  sta_netif = esp_netif_create_default_wifi_sta();
  if (sta_netif == nullptr) {
    logger::error("Failed to create default WiFi STA network interface");
    return false;
  }
  return true;
}

void WiFiAdapter::enableMDNS(const std::string& instance_name) {
  if (!mdns_initialized_) {
    const esp_err_t mdns_err = mdns_init();
    if (mdns_err != ESP_OK && mdns_err != ESP_ERR_INVALID_STATE) {
      logger::error("Error setting up mDNS responder: {}", esp_err_to_name(mdns_err));
      return;
    }
    mdns_initialized_ = true;
  }

  const esp_err_t host_err = mdns_hostname_set(hostname_.c_str());
  if (host_err != ESP_OK) {
    logger::warn("Failed to set mDNS hostname: {}", esp_err_to_name(host_err));
  }

  if (!instance_name.empty()) {
    const esp_err_t inst_err = mdns_instance_name_set(instance_name.c_str());
    if (inst_err != ESP_OK) {
      logger::warn("Failed to set mDNS instance name: {}", esp_err_to_name(inst_err));
    }
  }

  logger::info("mDNS responder active. Access device under {}.local", hostname_);
}

bool WiFiAdapter::isValidCredentials(std::string_view ssid, std::string_view password) noexcept {
  return !ssid.empty() && ssid.size() <= sizeof(wifi_sta_config_t::ssid) &&
         (password.empty() || (password.size() >= 8 && password.size() <= sizeof(wifi_sta_config_t::password)));
}

bool WiFiAdapter::startAccessPoint(std::string_view ssid, std::string_view password) {
  if (!initialized_) {
    logger::warn("Attempted to start Access Point without initialization");
    network_state_.setApFailed();
    return false;
  }

  wifi_config_t ap_config{};
  static_assert(sizeof(ap_config.ap.ssid) == sizeof(wifi_sta_config_t::ssid));
  static_assert(sizeof(ap_config.ap.password) == sizeof(wifi_sta_config_t::password));
  if (!isValidCredentials(ssid, password)) {
    logger::error("Invalid Access Point SSID or password length");
    network_state_.setApFailed();
    return false;
  }

  sta_started_.store(false, std::memory_order_release);
  network_state_.clearApFailed();
  network_state_.setStaDisconnected();

  if (!initAccessPoint()) {
    network_state_.setApFailed();
    return false;
  }

  esp_wifi_stop();  // ensure WiFi is stopped before changing mode

  std::copy_n(ssid.begin(), ssid.size(), ap_config.ap.ssid);
  ap_config.ap.ssid_len = static_cast<uint8_t>(ssid.size());

  if (password.empty()) {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  } else {
    std::copy_n(password.begin(), password.size(), ap_config.ap.password);
    ap_config.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;  // mixed mode for compatibility
  }

  ap_config.ap.max_connection = 4;
  ap_config.ap.channel = 1;
  ap_config.ap.ssid_hidden = 0;

  esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err == ESP_OK) {
    err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  }
  if (err == ESP_OK) {
    err = esp_wifi_start();
  }
  if (err != ESP_OK) {
    logger::error("Failed to start Access Point: {}", esp_err_to_name(err));
    network_state_.setApFailed();
    return false;
  }

  return true;
}

bool WiFiAdapter::connectToNetwork(std::string_view ssid, std::string_view password) {
  if (!initialized_) {
    logger::warn("Attempted to connect without initialization");
    network_state_.setStaFailed();
    return false;
  }

  wifi_config_t sta_config{};
  if (!isValidCredentials(ssid, password)) {
    logger::warn("Invalid WiFi credentials (empty SSID or invalid password length)");
    network_state_.setStaFailed();
    return false;
  }

  sta_started_.store(false, std::memory_order_release);
  network_state_.clearStaFailed();
  network_state_.setApDisconnected();

  if (!initStation()) {
    network_state_.setStaFailed();
    return false;
  }

  esp_wifi_stop();  // ensure WiFi is stopped before changing mode

  sta_config.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
  sta_config.sta.pmf_cfg.capable = true;
  sta_config.sta.pmf_cfg.required = false;

  std::copy_n(ssid.begin(), ssid.size(), sta_config.sta.ssid);
  std::copy_n(password.begin(), password.size(), sta_config.sta.password);

  esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err == ESP_OK) {
    err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  }
  if (err == ESP_OK) {
    const esp_err_t ps_err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // power save mode
    if (ps_err != ESP_OK) {
      logger::warn("Failed to set WiFi power save mode: {}", esp_err_to_name(ps_err));
    }
    const esp_err_t host_err = esp_netif_set_hostname(sta_netif, hostname_.c_str());
    if (host_err != ESP_OK) {
      logger::warn("Failed to set STA hostname: {}", esp_err_to_name(host_err));
    }
    err = esp_wifi_start();  // WIFI_EVENT_STA_START handler calls esp_wifi_connect()
  }
  if (err != ESP_OK) {
    logger::error("Failed to start WiFi STA connection: {}", esp_err_to_name(err));
    network_state_.setStaFailed();
    return false;
  }

  return true;
}

bool WiFiAdapter::reconnect() {
  if (!initialized_ || !sta_started_.load(std::memory_order_acquire)) {
    return false;
  }

  const esp_err_t err = esp_wifi_connect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {  // ESP_ERR_WIFI_CONN: already connecting
    logger::warn("WiFi reconnect failed: {}", esp_err_to_name(err));
    return false;
  }
  return true;
}

void WiFiAdapter::disconnectFromNetwork() {
  sta_started_.store(false, std::memory_order_release);
  if (!initialized_) {
    logger::warn("Attempted to disconnect without initialization");
    network_state_.setStaFailed();
    return;
  }

  const esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
    logger::warn("Failed to stop WiFi during disconnect: {}", esp_err_to_name(err));
  }
  network_state_.setStaFailed();

  logger::debug("Disconnected from Network");
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, StationConnectionCallback cb) {
  if (event != NetworkEvent::StationConnectionEstablished) {
    logger::warn("Attempted to register StationConnectionCallback for non-StationConnectionEstablished event.");
    return;
  }

  station_connection_callback_ = cb;
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, StationDisconnectionCallback cb) {
  if (event != NetworkEvent::StationDisconnected) {
    logger::warn("Attempted to register StationDisconnectionCallback for non-StationDisconnected event.");
    return;
  }

  station_disconnection_callback_ = cb;
}

void WiFiAdapter::registerEventHandler(NetworkEvent event, SimpleEventCallback cb) {
  switch (event) {
    case NetworkEvent::AccessPointStarted:
      access_point_started_callback_ = cb;
      break;
    case NetworkEvent::AccessPointStopped:
      access_point_stopped_callback_ = cb;
      break;
    default:
      logger::warn("Attempted to register SimpleEventCallback for unsupported event");
      break;
  }
}

void WiFiAdapter::espDispatchEvent(void* arg, const char* base, int32_t id, void* data) {
  if (arg != nullptr) {
    auto* adapter = static_cast<WiFiAdapter*>(arg);
    adapter->handleSystemEvent(base, id, data);
  }
}

void WiFiAdapter::handleSystemEvent(const char* base, int32_t id, void* data) {
  if (base == WIFI_EVENT) {
    switch (id) {
      case WIFI_EVENT_STA_START: {
        sta_started_.store(true, std::memory_order_release);
        const esp_err_t err = esp_wifi_connect();  // wifi should be connected as soon as it starts in station mode
        if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
          logger::error("WiFi connect failed on STA start: {}", esp_err_to_name(err));
          sta_started_.store(false, std::memory_order_release);
          network_state_.setStaFailed();
          if (station_disconnection_callback_ != nullptr) {
            station_disconnection_callback_(context_, WIFI_REASON_UNSPECIFIED, true);
          }
        }
        break;
      }
      case WIFI_EVENT_STA_DISCONNECTED: {
        network_state_.setStaDisconnected();
        if (!sta_started_.load(std::memory_order_acquire)) {
          break;
        }

        const auto* disconn = static_cast<const wifi_event_sta_disconnected_t*>(data);
        const std::uint8_t reason =
            disconn != nullptr ? disconn->reason : static_cast<std::uint8_t>(WIFI_REASON_UNSPECIFIED);
        if (station_disconnection_callback_ != nullptr) {
          station_disconnection_callback_(context_, reason, false);
        }
        break;
      }
      case WIFI_EVENT_AP_START:
        network_state_.setApConnected();

        if (access_point_started_callback_ != nullptr) {
          access_point_started_callback_(context_);
        }
        break;
      case WIFI_EVENT_AP_STOP:
        network_state_.setApDisconnected();

        if (access_point_stopped_callback_ != nullptr) {
          access_point_stopped_callback_(context_);
        }
        break;
      default:
        logger::debug("Unhandled WIFI_EVENT with id: {}", id);
    }
  } else if (base == IP_EVENT) {
    switch (id) {
      case IP_EVENT_STA_GOT_IP: {
        if (!sta_started_.load(std::memory_order_acquire)) {
          break;
        }
        auto* event_data = static_cast<ip_event_got_ip_t*>(data);
        app::network::IpAddress ip_address{event_data->ip_info.ip.addr};

        network_state_.setStaConnected();

        if (station_connection_callback_ != nullptr) {
          station_connection_callback_(context_, ip_address);
        }
        break;
      }
      case IP_EVENT_STA_LOST_IP:
        network_state_.setStaDisconnected();
        if (sta_started_.load(std::memory_order_acquire)) {
          logger::warn("STA lost IP address, reconnecting");
          esp_wifi_disconnect();
        }
        break;
      case IP_EVENT_AP_STAIPASSIGNED:
        break;
      default:
        logger::debug("Unhandled IP_EVENT with id: {}", id);
    }
  } else {
    logger::warn("Received event with unknown base: {}", base);
  }
}

}  // namespace app::network
