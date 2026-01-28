add_library(EngineRender STATIC
    renderer.h
    renderer.cpp
    
    lod/lod_system.h
    lod/lod_system.cpp
    
    culling/frustum_culling.h
    culling/frustum_culling.cpp
)

target_include_directories(EngineRender PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineRender PUBLIC
    EngineCore
    EnginePlatform
    Vulkan::Vulkan
)
