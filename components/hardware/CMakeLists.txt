idf_component_register(
    SRCS "SntpAdapter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES utils esp_netif lwip freertos
)
