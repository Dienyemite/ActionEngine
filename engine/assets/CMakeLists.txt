add_library(EngineAssets STATIC
    asset_manager.h
    asset_manager.cpp
    asset_importer.h
    asset_importer.cpp
    asset_hot_reloader.h
    asset_hot_reloader.cpp
    aeimport.h
    aeimport_parser.cpp
)

target_include_directories(EngineAssets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EngineAssets PUBLIC
    EngineCore
    EnginePlatform
    Vulkan::Vulkan
    assimp
    nlohmann_json::nlohmann_json)