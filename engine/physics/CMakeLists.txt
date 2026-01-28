add_library(EnginePhysics STATIC
    physics.h
    physics.cpp
)

target_include_directories(EnginePhysics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EnginePhysics PUBLIC
    EngineCore
)
