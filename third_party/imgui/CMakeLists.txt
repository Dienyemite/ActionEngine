# Dear ImGui library

add_library(ImGui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_impl_vulkan.cpp
    imgui_impl_win32.cpp
)

target_include_directories(ImGui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(ImGui PUBLIC Vulkan::Vulkan)

# Windows-specific
if(WIN32)
    target_compile_definitions(ImGui PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()
