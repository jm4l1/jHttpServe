cmake_minimum_required(VERSION 3.7)

add_definitions(-std=c++17)

set(CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS, "${CXX_FLAGS}")

add_subdirectory(lib/jjson)

include_directories(include)
file(GLOB SOURCES "")

project(capstoneHttp  ${SOURCES})

add_executable(capstoneHttp main.cpp)