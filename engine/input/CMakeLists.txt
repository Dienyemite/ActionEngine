add_library(EngineInput STATIC
    input_map.cpp
    input_map.h
    input.h
)

target_include_directories(EngineInput PUBLIC ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(EngineInput PUBLIC EngineCore EnginePlatform)
