#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace action {

class VulkanContext;

/*
 * Vulkan Swapchain
 * 
 * Triple buffering for smooth frame pacing
 * Handles window resize and recreation
 */

struct SwapchainConfig {
    u32 width;
    u32 height;
    bool vsync;
    u32 image_count = 3;  // Triple buffering
};

class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain() = default;
    
    bool Initialize(VulkanContext& context, const SwapchainConfig& config);
    void Shutdown(VulkanContext& context);
    
    // Recreate on resize
    bool Recreate(VulkanContext& context, u32 width, u32 height);
    
    // Frame operations
    VkResult AcquireNextImage(VkDevice device, VkSemaphore semaphore, u32& image_index);
    VkResult Present(VkQueue queue, VkSemaphore wait_semaphore, u32 image_index);
    
    // Accessors
    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    VkFormat GetFormat() const { return m_format; }
    VkExtent2D GetExtent() const { return m_extent; }
    u32 GetImageCount() const { return static_cast<u32>(m_images.size()); }
    
    const std::vector<VkImage>& GetImages() const { return m_images; }
    const std::vector<VkImageView>& GetImageViews() const { return m_image_views; }
    
    // Depth buffer
    VkImage GetDepthImage() const { return m_depth_image; }
    VkImageView GetDepthImageView() const { return m_depth_image_view; }
    VkFormat GetDepthFormat() const { return m_depth_format; }
    
private:
    bool CreateSwapchain(VulkanContext& context, const SwapchainConfig& config);
    bool CreateImageViews(VkDevice device);
    bool CreateDepthResources(VulkanContext& context);
    
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync);
    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height);
    VkFormat FindDepthFormat(VkPhysicalDevice device);
    
    void Cleanup(VkDevice device);
    
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format;
    VkExtent2D m_extent;
    
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_image_views;
    
    // Depth buffer
    VkImage m_depth_image = VK_NULL_HANDLE;
    VkDeviceMemory m_depth_memory = VK_NULL_HANDLE;
    VkImageView m_depth_image_view = VK_NULL_HANDLE;
    VkFormat m_depth_format;
    
    bool m_vsync = true;
};

} // namespace action
