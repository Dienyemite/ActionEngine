#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>

namespace action {

// Forward declarations
class VulkanContext;
class Renderer;
class Platform;

/*
 * ImGuiRenderer - Vulkan backend for Dear ImGui
 * 
 * Handles:
 * - ImGui context initialization
 * - Vulkan descriptor pool for ImGui
 * - Font texture upload
 * - Rendering ImGui draw data
 */

class ImGuiRenderer {
public:
    ImGuiRenderer() = default;
    ~ImGuiRenderer() = default;
    
    bool Initialize(VulkanContext& context, Renderer& renderer, Platform& platform);
    void Shutdown();
    
    void BeginFrame();
    void Render(VkCommandBuffer cmd);
    void EndFrame();
    
    // Recreate resources on swapchain resize
    void OnResize(u32 width, u32 height);
    
private:
    bool CreateDescriptorPool();
    bool InitImGuiVulkan(Renderer& renderer);
    bool UploadFonts();
    
    VulkanContext* m_context = nullptr;
    
    VkDescriptorPool m_imgui_pool = VK_NULL_HANDLE;
    
    bool m_initialized = false;
};

} // namespace action
