enable_testing()
file(GLOB SONIC_DATA 
    "${PROJECT_SOURCE_DIR}/testdata/**/*.json"
    "${PROJECT_SOURCE_DIR}/testdata/*.json"
)

file(COPY ${SONIC_DATA} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/corpus")
set(COMPILE_FLAGS -fsanitize=fuzzer,signed-integer-overflow,address -O1 -g -mavx2 -mpclmul -mbmi -mlzcnt)
set(LINK_FLAGS -fsanitize=fuzzer,signed-integer-overflow,address)

add_executable(fuzz "${PROJECT_SOURCE_DIR}/fuzz/fuzz.cpp")
target_include_directories(fuzz PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
target_compile_options(fuzz PRIVATE ${COMPILE_FLAGS})
target_link_libraries(fuzz PRIVATE ${LINK_FLAGS})
add_test(test_fuzz ${CMAKE_CURRENT_BINARY_DIR}/fuzz -max_total_time=300 ${CMAKE_CURRENT_BINARY_DIR}/corpus)
set_tests_properties(test_fuzz PROPERTIES TIMEOUT 600)

