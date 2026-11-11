# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "Application.cpp" "LifecycleManager.cpp" "HardwareConfigFacade.cpp" "RtcAppState.cpp"
    INCLUDE_DIRS "include"
    REQUIRES storage network utils esp_driver_gpio hardware
    PRIV_REQUIRES logger lwip
)
