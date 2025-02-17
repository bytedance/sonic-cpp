cmake_minimum_required(VERSION 3.11)

project(
  sonic_cpp
  VERSION 1.0.0
  DESCRIPTION "A fast JSON serializing & deserializing library, accelerated by SIMD."
  LANGUAGES CXX)

option(BUILD_UNITTEST "Build unittest." ON)
option(BUILD_FUZZ "Build fuzz." OFF)
option(BUILD_BENCH "Build benchmark." OFF)
option(ENABLE_SVE2_128 "Build for Arm SVE2 with 128 bit vector size" OFF)
option(ENABLE_RVV_128 "Build for RISC-V RVV with 128 bit vector size" ON)

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
