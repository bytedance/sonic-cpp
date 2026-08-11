file(GLOB SONIC_EXAMPLE_FILES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

include("${PROJECT_SOURCE_DIR}/cmake/set_arch_flags.cmake")

foreach(EXAMPLE_FILE IN LISTS SONIC_EXAMPLE_FILES)
  get_filename_component(EXAMPLE_NAME "${EXAMPLE_FILE}" NAME_WE)
  set(EXAMPLE_TARGET "sonic_example_${EXAMPLE_NAME}")

  add_executable(${EXAMPLE_TARGET} "${EXAMPLE_FILE}")
  target_compile_features(${EXAMPLE_TARGET} PRIVATE cxx_std_17)
  target_include_directories(${EXAMPLE_TARGET}
                             PRIVATE "${PROJECT_SOURCE_DIR}/include")
  set_arch_flags(${EXAMPLE_TARGET} "${CMAKE_SYSTEM_PROCESSOR}")
endforeach()
