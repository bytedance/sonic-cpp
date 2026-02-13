cmake_minimum_required(VERSION 3.11)
include(FetchContent)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
    macro(FetchContent_MakeAvailable NAME)
        FetchContent_GetProperties(${NAME})
        if(NOT ${NAME}_POPULATED)
            FetchContent_Populate(${NAME})
            add_subdirectory(${${NAME}_SOURCE_DIR} ${${NAME}_BINARY_DIR})
        endif()
    endmacro()
endif()

FetchContent_Declare(
    gflags
    GIT_REPOSITORY https://github.com/gflags/gflags.git
    GIT_TAG  v2.2.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(gflags)

# set(SIMDJSON_AVX512_ALLOWED OFF)
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG  v4.2.4
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(simdjson)
if(NOT simdjson_POPULATED)
    FetchContent_Populate(simdjson)
endif()
add_library(simdjson INTERFACE)
target_compile_definitions(simdjson INTERFACE SIMDJSON_AVX512_ALLOWED=0)
target_sources(simdjson INTERFACE ${simdjson_SOURCE_DIR}/singleheader/simdjson.cpp)
target_include_directories(simdjson INTERFACE ${simdjson_SOURCE_DIR}/singleheader)

FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG  24b5e7a8b27f42fa16b96fc70aade9106cf7102f
)
FetchContent_Populate(rapidjson)

FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG  v1.7.19
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(cjson)

FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG  0.10.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(yyjson)

FetchContent_Declare(
    jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG  1.9.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(jsoncpp)
