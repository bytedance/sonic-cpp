if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Skipping fuzz target: Requires Clang compiler (currently using ${CMAKE_CXX_COMPILER_ID})")
    return()
endif()

enable_testing()

file(GLOB SONIC_DATA 
    "${PROJECT_SOURCE_DIR}/testdata/**/*.json"
    "${PROJECT_SOURCE_DIR}/testdata/*.json"
)

include("${PROJECT_SOURCE_DIR}/cmake/set_arch_flags.cmake")

file(COPY ${SONIC_DATA} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/corpus")

set(FUZZ_FLAGS -fsanitize=fuzzer,signed-integer-overflow,address)

add_executable(fuzz "${PROJECT_SOURCE_DIR}/fuzz/fuzz.cpp")
target_include_directories(fuzz PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
target_compile_options(fuzz PRIVATE ${FUZZ_FLAGS} -O1 -g)
set_arch_flags(fuzz ${CMAKE_SYSTEM_PROCESSOR})
target_link_options(fuzz PRIVATE ${FUZZ_FLAGS})

if(APPLE)
    target_compile_options(fuzz PRIVATE -mmacosx-version-min=10.15)
endif()

add_test(test_fuzz ${CMAKE_CURRENT_BINARY_DIR}/fuzz -max_total_time=300 ${CMAKE_CURRENT_BINARY_DIR}/corpus)
set_tests_properties(test_fuzz PROPERTIES TIMEOUT 600)

