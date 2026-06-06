# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "WiFiAdapter.cpp" "NetworkManager.cpp" "DnsServer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif esp_wifi esp_event freertos mdns lwip utils espressif__fmt
    PRIV_REQUIRES logger
)
