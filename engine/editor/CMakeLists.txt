# Editor module

add_library(EngineEditor STATIC
    editor.cpp
    editor.h
    imgui_renderer.cpp
    imgui_renderer.h
    panels/viewport_panel.cpp
    panels/viewport_panel.h
    panels/multi_viewport_panel.cpp
    panels/multi_viewport_panel.h
    panels/scene_tree_panel.cpp
    panels/scene_tree_panel.h
    panels/inspector_panel.cpp
    panels/inspector_panel.h
    panels/console_panel.cpp
    panels/console_panel.h
    panels/gizmo_panel.cpp
    panels/gizmo_panel.h
    commands/command.cpp
    commands/command.h
    commands/editor_commands.cpp
    commands/editor_commands.h
    prefabs/prefab.cpp
    prefabs/prefab.h
    shader_graph/shader_graph.cpp
    shader_graph/shader_graph.h
    shader_graph/shader_graph_editor.cpp
    shader_graph/shader_graph_editor.h
)

target_include_directories(EngineEditor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(EngineEditor PUBLIC
    EngineCore
    EnginePlatform
    EngineRender
    ImGui
)
