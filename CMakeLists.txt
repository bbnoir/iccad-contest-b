cmake_minimum_required(VERSION 2.8.12)

project(ContestB)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ContestB_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ContestB_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ContestB_SOURCE_DIR}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic -std=c++11")

add_subdirectory(src)