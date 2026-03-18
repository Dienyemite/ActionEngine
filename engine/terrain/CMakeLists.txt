add_library(EngineTerrain STATIC
    terrain.h
    terrain.cpp
)

target_include_directories(EngineTerrain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineTerrain PUBLIC
    EngineCore
    EngineAssets
    EngineRender
)
