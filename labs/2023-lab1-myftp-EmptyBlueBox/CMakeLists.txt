project(2023-lab1-myFTP)
cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(ftp_client_functions src/ftp_client_functions.cpp)
add_library(ftp_client_utils src/ftp_client_utils.cpp)
add_executable(ftp_client src/ftp_client.cpp)
target_link_libraries(ftp_client ftp_client_functions ftp_client_utils)

add_library(ftp_server_functions src/ftp_server_functions.cpp)
add_library(ftp_server_utils src/ftp_server_utils.cpp)
add_executable(ftp_server src/ftp_server.cpp)
target_link_libraries(ftp_server ftp_server_functions ftp_server_utils)