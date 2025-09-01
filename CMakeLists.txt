cmake_minimum_required(VERSION 3.12)
project(player)

add_executable(player src/main.cpp)

find_package(PkgConfig)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

target_include_directories(player PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
target_link_libraries(player ${PORTAUDIO_LIBRARIES})
target_compile_options(player PUBLIC --std=c++17 -Wall -O3)
