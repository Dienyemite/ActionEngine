add_library(EngineWorld STATIC
    world_manager.h
    world_manager.cpp
)

target_include_directories(EngineWorld PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineWorld PUBLIC
    EngineCore
    EngineRender
)
