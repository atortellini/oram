cmake_minimum_required(VERSION 3.10)
project(ORAM C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

find_package(OpenSSL REQUIRED)

include_directories(parameters)

file(GLOB PATH_ORAM_SRCS "path_oram/src/*.c")

add_executable(path_oram_test ${PATH_ORAM_SRCS})

target_link_libraries(path_oram_test OpenSSL::Crypto)

file(GLOB RORAM_SRCS "roram/src/*.c")

add_executable(roram_test ${RORAM_SRCS})

target_link_libraries(roram_test OpenSSL::Crypto)