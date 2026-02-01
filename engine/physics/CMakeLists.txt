add_library(EnginePhysics STATIC
    physics.h
    physics.cpp
    collision_shapes.h
    physics_world.h
    physics_world.cpp
    character_controller.h
    character_controller.cpp
    jolt/jolt_physics.h
    jolt/jolt_physics.cpp
    jolt/jolt_layers.h
    jolt/jolt_debug_renderer.h
    jolt/jolt_debug_renderer.cpp
)

target_include_directories(EnginePhysics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EnginePhysics PUBLIC
    EngineCore
    EngineGameplay
    Jolt
)
