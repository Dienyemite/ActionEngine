cmake_minimum_required(VERSION 3.20)
project(ActionEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(ENGINE_BUILD_TESTS "Build engine tests" ON)
option(ENGINE_BUILD_EDITOR "Build editor tools" ON)
option(ENGINE_ENABLE_PROFILING "Enable profiling markers" ON)
option(ENGINE_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS=1)
    add_definitions(-DNOMINMAX)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DPLATFORM_LINUX=1)
endif()

# Find Vulkan
find_package(Vulkan REQUIRED)

# Third-party dependencies
add_subdirectory(third_party)

# Engine modules
add_subdirectory(engine/core)
add_subdirectory(engine/platform)
add_subdirectory(engine/input)
add_subdirectory(engine/render)
add_subdirectory(engine/scene)
add_subdirectory(engine/resources)
add_subdirectory(engine/world)
add_subdirectory(engine/assets)
add_subdirectory(engine/physics)
add_subdirectory(engine/audio)
add_subdirectory(engine/gameplay)
add_subdirectory(engine/scripting)
add_subdirectory(engine/serialization)
add_subdirectory(engine/editor)
add_subdirectory(engine/project)

# Main engine library
add_library(ActionEngine STATIC
    engine/engine.cpp
    engine/engine.h
)

target_link_libraries(ActionEngine PUBLIC
    EngineCore
    EnginePlatform
    EngineInput
    EngineRender
    EngineScene
    EngineResources
    EngineSerialization
    EngineWorld
    EngineAssets
    EnginePhysics
    EngineAudio
    EngineGameplay
    EngineScripting
    EngineEditor
    EngineProject
)

target_include_directories(ActionEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Game executable
add_executable(Game
    game/main.cpp
)

target_link_libraries(Game PRIVATE ActionEngine)

# Copy assets and shaders
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Game>/assets
)

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders/compiled $<TARGET_FILE_DIR:Game>/shaders
)
