cmake_minimum_required(VERSION 3.7)
project(capstoneHttp)

add_definitions(-std=c++17)

set(CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS, "${CXX_FLAGS}")

add_subdirectory(lib/jjson)

include_directories(include "lib/jjson/include")
file(GLOB SOURCES "src/*.cpp" "lib/jjson/src/*.cpp")

target_link_libraries ( jjson )
add_executable(capstoneHttp main.cpp ${SOURCES})