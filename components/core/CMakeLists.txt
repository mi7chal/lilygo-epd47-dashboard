# FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "AppCore.cpp" 
    INCLUDE_DIRS "include"
    REQUIRES storage network 
    PRIV_REQUIRES logger utils lwip
)
