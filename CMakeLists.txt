# Not actually sure what the minimum required version is.
cmake_minimum_required(VERSION 2.8)

project(ASSInspector C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wunreachable-code -Wfloat-equal -Wredundant-decls -Winit-self -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

option(USE_ZLIB "Use zlib for crc computation if available." ON)

add_subdirectory(src)
