# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "WiFiAdapter.cpp" "NetworkManager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif esp_wifi freertos utils
    PRIV_REQUIRES logger
)
