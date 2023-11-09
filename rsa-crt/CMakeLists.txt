# ATTENTION: this file exists only to make CLion code highlighting work and **NOT to compile** something
cmake_minimum_required(VERSION 3.13)
project(unnamed C)

set(CMAKE_C_STANDARD 11)

set(IPP_BUILD_FOLDER /ipp-crypto/_build/.build/RELEASE)

message(STATUS ${CMAKE_SOURCE_DIR}${IPP_BUILD_FOLDER}/include)
include_directories(${CMAKE_SOURCE_DIR}${IPP_BUILD_FOLDER}/include)
link_directories(${CMAKE_SOURCE_DIR}${IPP_BUILD_FOLDER}/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -pedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result")
add_executable(output main.c)
