cmake_minimum_required(VERSION 3.20)

include(CTest)
include(FetchContent)

# Catch2's CMake helpers (catch_discover_tests) live in its extras/ folder.
# FetchContent_GetProperties reads the populated source dir from global state
# set when FetchContent_MakeAvailable(Catch2) ran inside add_subdirectory(third_party).
FetchContent_GetProperties(Catch2 SOURCE_DIR _catch2_src)
list(APPEND CMAKE_MODULE_PATH "${_catch2_src}/extras")
include(Catch)

# Common include path shared by all test targets
set(ENGINE_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}
)

add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(e2e)
