# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "Timer.cpp" "TaskRunner.cpp" "logger.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos log espressif__fmt
)
