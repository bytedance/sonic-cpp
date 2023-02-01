file(GLOB BENCH_SRC
"${PROJECT_SOURCE_DIR}/benchmark/main.cpp"
"${PROJECT_SOURCE_DIR}/benchmark/*.h"
"${PROJECT_SOURCE_DIR}/benchmark/*.hpp"
)

set(COMPILE_FLAGS -O3 -DNDEBUG -g -mavx2 -mpclmul -mbmi -mlzcnt)

include("${PROJECT_SOURCE_DIR}/cmake/external.cmake")

add_executable(bench "${BENCH_SRC}")

set(EP_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/external/)
include(ExternalProject)
ExternalProject_Add(
    google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    PREFIX ${EP_PREFIX}
    GIT_TAG  main
    CMAKE_ARGS -DBENCHMARK_ENABLE_TESTING=OFF -DCMAKE_BUILD_TYPE=Release 
    INSTALL_COMMAND ""
)
add_dependencies(bench google_benchmark)

target_compile_features(bench PRIVATE cxx_std_17)
target_link_libraries(bench simdjson yyjson cjson benchmark benchmark_main pthread stdc++fs)
target_include_directories(bench 
    PRIVATE ${rapidjson_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR} 
    PRIVATE ${EP_PREFIX}/src/google_benchmark/include/
    PRIVATE ${cjson_SOURCE_DIR}/
    PRIVATE ${yyjson_SOURCE_DIR}/src/
    PRIVATE ${simdjson_SOURCE_DIR}/singleheader/
)
target_link_directories(bench 
    PRIVATE ${EP_PREFIX}/src/google_benchmark-build/src
    PRIVATE ${simdjson_SOURCE_DIR}/
)
target_compile_options(bench PRIVATE ${COMPILE_FLAGS})
