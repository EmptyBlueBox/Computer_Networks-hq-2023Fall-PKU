cmake_minimum_required(VERSION 3.11)
project(CompNet-Lab4 LANGUAGES C CXX)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)

include_directories(${CMAKE_SOURCE_DIR})
get_property(dirs DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "include_dirs = ${dirs}")

find_library(LIB_LAB4RUNTIME lab4runtime build/)

# library : switch
add_library(switch STATIC switch.cc)

# executable : simulator
add_executable(simulator simulator.cc)
target_link_libraries(simulator ${LIB_LAB4RUNTIME} switch)
