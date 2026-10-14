cmake_minimum_required(VERSION 3.11)

project(sonic_android_example LANGUAGES CXX)

if(NOT ANDROID)
  message(FATAL_ERROR "Configure this example with the Android NDK toolchain")
endif()

add_library(sonic_android_example SHARED sonic_android.cpp)
target_compile_features(sonic_android_example PRIVATE cxx_std_17)
target_include_directories(
  sonic_android_example PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")
