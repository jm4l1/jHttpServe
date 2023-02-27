cmake_minimum_required(VERSION 3.7)
set(project jHttpServe)
project(jHttpServe)

add_definitions(-std=c++20)

set(CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS, "${CXX_FLAGS}")
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(lib/jjson)
add_subdirectory(tests)

include_directories(include "lib/jjson/include")
file(GLOB SOURCES "src/*.cpp" "lib/jjson/src/*.cpp")

add_executable(${project} main.cpp ${SOURCES})
target_link_libraries( ${project} ${CMAKE_THREAD_LIBS_INIT} )