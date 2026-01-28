add_library(EngineGameplay STATIC
    ecs/ecs.h
    ecs/ecs.cpp
)

target_include_directories(EngineGameplay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineGameplay PUBLIC
    EngineCore
)
