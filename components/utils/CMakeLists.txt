# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "Timer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer 
)
