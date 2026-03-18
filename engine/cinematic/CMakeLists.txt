add_library(EngineCinematic STATIC
    cinematic.h
    cinematic.cpp
)

target_include_directories(EngineCinematic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineCinematic PUBLIC
    EngineCore
    EngineGameplay
    EngineRender
)
