# copied from https://github.com/google/googletest/blob/main/googletest/README.md
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
  googletest
  # Specify the commit you depend on and update it regularly.
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# for arm CI: ASAN is very annoying when dealing with qemu
option(ENABLE_ASAN "Enable AddressSanitizer" ON)

file(GLOB SONIC_TEST_FILES
     "${PROJECT_SOURCE_DIR}/tests/*.h"
     "${PROJECT_SOURCE_DIR}/tests/*.cpp"
)
add_executable(unittest ${SONIC_TEST_FILES})
target_compile_features(unittest PRIVATE cxx_std_17)
target_link_libraries(unittest PRIVATE gtest_main)
target_include_directories(unittest PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})

if(ENABLE_ASAN)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(unittest PRIVATE -O0 -g -fsanitize=address -Werror -Wall -Wextra-semi -Wextra-semi-stmt -Wcomma)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(unittest PRIVATE -O0 -g -fsanitize=address -Werror -Wall)
    endif()
    target_link_options(unittest PRIVATE -fsanitize=address)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(unittest PRIVATE -O0 -g -Werror -Wall -Wextra-semi -Wextra-semi-stmt -Wcomma)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(unittest PRIVATE -O0 -g -Werror -Wall -Wno-use-after-free)
    endif()
endif()

include("${PROJECT_SOURCE_DIR}/cmake/set_arch_flags.cmake")
set_arch_flags(unittest ${CMAKE_SYSTEM_PROCESSOR})
add_test(NAME sonic-unittest COMMAND unittest)
