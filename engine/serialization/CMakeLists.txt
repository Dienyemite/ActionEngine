add_library(EngineSerialization STATIC
    serialization.cpp
    json_format.cpp
)

target_include_directories(EngineSerialization PUBLIC ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(EngineSerialization PUBLIC EngineCore)
