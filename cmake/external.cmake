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
    GIT_TAG  master
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(gflags)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG  v1.0.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(simdjson)
target_compile_definitions(simdjson PUBLIC SIMDJSON_EXCEPTIONS=OFF)

FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG  master
)
FetchContent_Populate(rapidjson)

FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG  master
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(cjson)

FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG  master
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(yyjson)

