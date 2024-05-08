cmake_minimum_required(VERSION 3.18)
project(assignment2-rtp)

enable_testing()
add_subdirectory(third_party/googletest-release-1.12.1)
include_directories(third_party/googletest-release-1.12.1/googletest/include)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE "Debug")
add_compile_options("-Wall")

find_package(Threads REQUIRED)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

add_compile_definitions(SOURCE_DIR="${CMAKE_SOURCE_DIR}")

option(DL "debug logging" OFF)

if(DL)
    message("Debug logging is on")
    add_compile_definitions(LDEBUG)
endif()

unset(DL CACHE)

include_directories(${CMAKE_SOURCE_DIR}/src/include)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)
include(GoogleTest)

#定义库
add_library(util_lib src/util.cpp)
add_library(rtp_lib src/rtp.cpp)
add_library(sender_functions_lib src/sender_functions.cpp)
add_library(receiver_functions.lib src/receiver_functions.cpp)

target_link_libraries(rtp_lib PUBLIC util_lib)
target_link_libraries(sender_functions_lib PUBLIC rtp_lib)
target_link_libraries(receiver_functions.lib PUBLIC rtp_lib)

#发送端
add_executable(sender src/sender.cpp)
target_link_libraries(sender sender_functions_lib)

#接收端
add_executable(receiver src/receiver.cpp)
target_link_libraries(receiver receiver_functions.lib)

add_executable(rtp_test_all src/test.cpp)
target_link_libraries(rtp_test_all PUBLIC Threads::Threads GTest::gtest_main)
target_link_libraries(rtp_test_all PUBLIC util_lib)

gtest_discover_tests(rtp_test_all)
