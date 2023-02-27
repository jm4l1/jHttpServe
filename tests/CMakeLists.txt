cmake_minimum_required(VERSION 3.7)

# Set the C++ standard we will use
set(CMAKE_CXX_STANDARD 20)

set(this tests)

project(${this} C CXX)

file(GLOB SOURCES "src/*.cpp")

add_executable(${this} ${SOURCES} "lib/catch.hpp")

include_directories(include "../include")

set_target_properties(${this} PROPERTIES FOLDER tests)

target_include_directories(${this} PRIVATE)

add_test(
    NAME ${this}
    COMMAND ${this}
)
