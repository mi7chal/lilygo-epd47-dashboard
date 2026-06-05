#pragma once

#include <esp_netif.h>
#include <esp_wifi.h>

#include <functional>
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
  AccessPointStarted = WIFI_EVENT_AP_STACONNECTED,
  AccessPointStopped = WIFI_EVENT_AP_STADISCONNECTED,
};

class WiFiAdapter {
 public:
  // Callback types for different events
  // Splitting them into separate types for better type safety and clarity
  // pointers instead of std::function to avoid dynamic memory allocation and
  // adiitional cpu/memory overhead
  using StationConnectionCallback = void (*)(void* context, IpAddress ip_address);
  using StationDisconnectionCallback = void (*)(void* context);
  using AccessPointStartedCallback = void (*)(void* context);
  using AccessPointStoppedCallback = void (*)(void* context);

  WiFiAdapter(void* context, NetworkState& network_state, std::string_view hostname);
  ~WiFiAdapter() noexcept;
  WiFiAdapter(const WiFiAdapter&) = delete;
  WiFiAdapter& operator=(const WiFiAdapter&) = delete;
  WiFiAdapter(WiFiAdapter&& other) noexcept;
  WiFiAdapter& operator=(WiFiAdapter&& other) noexcept;

  bool init();
  void deinit();

  void enableMDNS(std::string_view instance_name = "");

  void startAccessPoint(std::string_view ssid, std::string_view password);
  void stopAccessPoint();

  void connectToWiFi(std::string_view ssid, std::string_view password);
  void disconnectFromWiFi();

  void registerEventHandler(NetworkEvent event, StationConnectionCallback cb);
  void registerEventHandler(NetworkEvent event, StationDisconnectionCallback cb);
  void registerEventHandler(NetworkEvent event, AccessPointStartedCallback cb);
  void registerEventHandler(NetworkEvent event, AccessPointStoppedCallback cb);

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

  void* context_{nullptr};
  NetworkState& network_state_;

  bool initialized_{false};
  esp_netif_t* sta_netif{nullptr};  // nullptr means not initialized
  esp_netif_t* ap_netif{nullptr};   // same as above

  EventGroupHandle_t wifi_event_group_{nullptr};

  std::string_view hostname_;

  // Callbacks for different events
  // Using separate fields for each callback is more memory-efficient (no
  // dynamic memory, no containers overhead)
  StationConnectionCallback stationConnectionCallback_{nullptr};
  StationDisconnectionCallback stationDisconnectionCallback_{nullptr};
  AccessPointStartedCallback accessPointStartedCallback_{nullptr};
  AccessPointStoppedCallback accessPointStoppedCallback_{nullptr};

  esp_event_handler_instance_t wifi_event_instance;
  esp_event_handler_instance_t ip_event_instance;

  static void esp_event_dispatcher(void* arg, const char* base, int32_t id, void* data);
  void handle_system_event(const char* base, int32_t id, void* data);
};

}  // namespace app::network