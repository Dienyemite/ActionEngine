add_library(EngineAnimation STATIC
    skeleton.h
    animation_clip.h
    animation_clip.cpp
    animation_player.h
    animation_player.cpp
    animation_state_machine.h
    animation_state_machine.cpp
    skinned_mesh.h
    skinned_mesh.cpp
    animation_library.h
    animation_library.cpp
)

target_include_directories(EngineAnimation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineAnimation PUBLIC
    EngineCore
    EngineGameplay
)
