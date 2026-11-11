idf_component_register(
    SRCS "SntpAdapter.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES logger
    REQUIRES utils esp_netif lwip freertos
)
