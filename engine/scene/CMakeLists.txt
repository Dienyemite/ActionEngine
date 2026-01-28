add_library(EngineScene STATIC
    node.cpp
    node.h
    node_3d.cpp
    node_3d.h
    scene_tree.cpp
    scene_tree.h
    scene.h
)

target_include_directories(EngineScene PUBLIC ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(EngineScene PUBLIC EngineCore)
