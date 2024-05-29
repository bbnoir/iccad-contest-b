cmake_minimum_required(VERSION 2.18)

project(ContestB)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ContestB_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ContestB_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ContestB_SOURCE_DIR}/lib)

add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_17)
target_compile_options(compiler_flags INTERFACE "-O3" "-Wall" "-Wextra" "-Wpedantic" "-Werror")

add_subdirectory(src)