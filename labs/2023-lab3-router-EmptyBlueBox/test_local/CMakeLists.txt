
cmake_policy(SET CMP0048 NEW)

project(lab3_test)
cmake_minimum_required(VERSION 3.11)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
set(gtest_force_shared_crt ON)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
add_executable(lab3_test test.cpp)
target_link_libraries(lab3_test GTest::gtest_main)
gtest_discover_tests(lab3_test)