add_library(EngineScripting STATIC
    script.h
    script.cpp
    script_system.h
    script_system.cpp
)

target_include_directories(EngineScripting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EngineScripting PUBLIC
    EngineCore
    EngineGameplay
)
