add_library(EngineNavigation STATIC
    navmesh.h
    navmesh.cpp
)

target_include_directories(EngineNavigation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineNavigation PUBLIC
    EngineCore
    EngineGameplay
)
