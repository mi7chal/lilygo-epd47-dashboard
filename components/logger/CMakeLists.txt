# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "logger.cpp"
    INCLUDE_DIRS "include"
    REQUIRES log espressif__fmt
)
