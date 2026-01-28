# Third-party dependencies

# Dear ImGui for editor UI
add_subdirectory(imgui)

# Create interface target for third-party libraries
add_library(ThirdParty INTERFACE)
target_link_libraries(ThirdParty INTERFACE ImGui)
