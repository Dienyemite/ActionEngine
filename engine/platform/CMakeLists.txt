add_library(EnginePlatform STATIC
    platform.h
    platform.cpp
    
    vulkan/vulkan_context.h
    vulkan/vulkan_context.cpp
    vulkan/vulkan_device.h
    vulkan/vulkan_device.cpp
    vulkan/vulkan_swapchain.h
    vulkan/vulkan_swapchain.cpp
)

target_include_directories(EnginePlatform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(EnginePlatform PUBLIC
    EngineCore
    Vulkan::Vulkan
    ImGui
)

if(WIN32)
    target_link_libraries(EnginePlatform PRIVATE
        user32
        gdi32
    )
endif()
