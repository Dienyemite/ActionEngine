add_library(EngineResources STATIC
    resource.cpp
    resource.h
    resource_cache.cpp
    resource_cache.h
    resource_loader.cpp
    resource_loader.h
    resource_types.cpp
    resource_types.h
    resources.h
)

target_include_directories(EngineResources PUBLIC ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(EngineResources PUBLIC EngineCore)
