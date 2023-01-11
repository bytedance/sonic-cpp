cmake_minimum_required(VERSION 3.11)

project(
  sonic_cpp
  VERSION 1.0.0
  DESCRIPTION "A fast JSON serializing & deserializing library, accelerated by SIMD."
  LANGUAGES CXX)

option(BUILD_UNITTEST "Build unittest." ON)
option(BUILD_FUZZ "Build fuzz." OFF)
option(BUILD_BENCH "Build benchmark." OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
if(BUILD_UNITTEST)
  add_subdirectory(tests)
endif()

if(BUILD_FUZZ)
  add_subdirectory(fuzz)
endif()

if(BUILD_BENCH)
  add_subdirectory(benchmark)
endif()
