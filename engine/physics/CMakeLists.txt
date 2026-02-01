add_library(EnginePhysics STATIC
    physics.h
    physics.cpp
    collision_shapes.h
    physics_world.h
    physics_world.cpp
    character_controller.h
    character_controller.cpp
)

target_include_directories(EnginePhysics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EnginePhysics PUBLIC
    EngineCore
    EngineGameplay
)
