add_library(EngineAudio STATIC
    audio.h
    audio.cpp
)

target_include_directories(EngineAudio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineAudio PUBLIC
    EngineCore
)
