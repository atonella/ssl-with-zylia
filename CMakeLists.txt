cmake_minimum_required(VERSION 3.10)
project(ssl-with-zylia)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ALSA
find_package(ALSA REQUIRED)

# SSL Library
add_library(ssl_lib STATIC
    src/ssl/sound_source_localization.cpp
    src/ssl/utils.cpp
)

target_include_directories(ssl_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(ssl_lib PUBLIC
    ${ALSA_LIBRARIES}
)

# Peak Volume Monitor Example
add_executable(peak_volume_monitor
    examples/ssl/peak_volume_monitor.cpp
)

target_link_libraries(peak_volume_monitor
    ssl_lib
    ${ALSA_LIBRARIES}
)
