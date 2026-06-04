# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "NVSAdapter.cpp" "StorageManager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
    PRIV_REQUIRES logger
)
