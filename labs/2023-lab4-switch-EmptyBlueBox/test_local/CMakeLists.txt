cmake_minimum_required(VERSION 3.11)
project(CompNet-Lab4 LANGUAGES C CXX)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)

# Fetch Content : GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  #GIT_REPOSITORY git@github.com:google/googletest.git
  GIT_TAG release-1.12.1
)
set(gtest_force_shared_crt ON)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
add_executable(lab4_test test.cc)
target_link_libraries(lab4_test GTest::gtest_main)
gtest_discover_tests(lab4_test)
