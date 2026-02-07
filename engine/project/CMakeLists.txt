# ActionEngine Project Management Module

add_library(EngineProject STATIC
    project.cpp
    project.h
    scene_serializer.cpp
    scene_serializer.h
)

target_include_directories(EngineProject PUBLIC
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(EngineProject
    EngineCore
    EngineSerialization
    EngineGameplay
    EngineEditor
    EnginePhysics
)

# Set C++20 standard
target_compile_features(EngineProject PUBLIC cxx_std_20)
