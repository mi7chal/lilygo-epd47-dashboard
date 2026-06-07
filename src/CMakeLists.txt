# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
	SRCS ${app_sources}
	REQUIRES core esp_http_client esp_http_server esp-tls mdns json vroland__epdiy logger
)
