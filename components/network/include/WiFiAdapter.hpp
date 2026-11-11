#pragma once

#include <esp_netif.h>
#include <esp_wifi.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "IpAddress.hpp"
#include "NetworkState.hpp"
namespace app::network {


struct WiFiConnectionInfo {
  std::string ssid;
  std::string hostname;
  IpAddress ip;
  int32_t rssi;
};

struct AccessPointInfo {
  std::string ssid;
  std::string password;
  std::string hostname;
  IpAddress ip;
};

enum class NetworkEvent {
  StationConnectionEstablished = IP_EVENT_STA_GOT_IP,  // we consider connection established when we get IP
                                                       // address, not when we connect to AP
  StationDisconnected = WIFI_EVENT_STA_DISCONNECTED,
  AccessPointStarted = WIFI_EVENT_AP_START,
  AccessPointStopped = WIFI_EVENT_AP_STOP,
};

class WiFiAdapter {
 public:
  // Callback types for different events
  // Splitting them into separate types for better type safety and clarity
  // pointers instead of std::function to avoid dynamic memory allocation and
  // additional cpu/memory overhead
  using StationConnectionCallback = void (*)(void* context, IpAddress ip_address);
  using SimpleEventCallback = void (*)(void* context);

  WiFiAdapter(void* context, NetworkState& network_state, std::string hostname);
  ~WiFiAdapter() noexcept;
  WiFiAdapter(const WiFiAdapter&) = delete;
  WiFiAdapter& operator=(const WiFiAdapter&) = delete;
  WiFiAdapter(WiFiAdapter&& other) = delete;
  WiFiAdapter& operator=(WiFiAdapter&& other) = delete;

  bool init();
  void deinit();

  void enableMDNS(const std::string& instance_name = "");

  void startAccessPoint(std::string_view ssid, std::string_view password);
  void stopAccessPoint();

  void connectToNetwork(std::string_view ssid, std::string_view password);
  /// retries the current connection attempt without reconfiguring WiFi
  void reconnect();
  void disconnectFromNetwork();

  void registerEventHandler(NetworkEvent, SimpleEventCallback);
  void registerEventHandler(NetworkEvent, StationConnectionCallback);

  /**
   * @brief unregisters (resets) handler for given event
   *
   * @param event handler event
   */
  void unregisterEventHandler(NetworkEvent event);

  [[nodiscard]] bool isConnected() const { return network_state_.isConnected(); };
  [[nodiscard]] bool isInitialized() const { return initialized_; };
  [[nodiscard]] std::optional<WiFiConnectionInfo> getWiFiConnectionInfo() const;
  [[nodiscard]] std::optional<AccessPointInfo> getAccessPointInfo() const;

 private:
  void initAccessPoint();
  void initStation();
  void unregisterEspHandlers();

  void* context_{nullptr};
  NetworkState& network_state_;

  bool initialized_{false};
  esp_netif_t* sta_netif{nullptr};  // nullptr means not initialized
  esp_netif_t* ap_netif{nullptr};   // same as above

  std::string hostname_;
  std::string portal_uri_;  // DHCP option 114 keeps the pointer, so it must outlive the DHCP server

  // Callbacks for different events
  // Using separate fields for each callback is more memory-efficient (no
  // dynamic memory, no containers overhead)
  StationConnectionCallback station_connection_callback_{nullptr};
  SimpleEventCallback station_disconnection_callback_{nullptr};
  SimpleEventCallback access_point_started_callback_{nullptr};
  SimpleEventCallback access_point_stopped_callback_{nullptr};

  esp_event_handler_instance_t wifi_event_delegating_handler_{nullptr};
  esp_event_handler_instance_t ip_event_delegating_handler_{nullptr};

  static void espDispatchEvent(void* arg, const char* base, int32_t id, void* data);
  void handleSystemEvent(const char* base, int32_t id, void* data);
};

}  // namespace app::network