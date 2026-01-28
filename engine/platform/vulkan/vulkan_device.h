#pragma once

// Stub header for vulkan_device.h
// GPU resource management will go here

#include "core/types.h"
#include <vulkan/vulkan.h>

namespace action {

// Forward declarations
class VulkanContext;

// GPU Buffer
struct GPUBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

// GPU Image/Texture
struct GPUImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    u32 width = 0;
    u32 height = 0;
    u32 mip_levels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

class VulkanDevice {
public:
    bool Initialize(VulkanContext& context);
    void Shutdown();
    
    // Buffer operations
    GPUBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                           VkMemoryPropertyFlags properties);
    void DestroyBuffer(GPUBuffer& buffer);
    
    // Image operations
    GPUImage CreateImage(u32 width, u32 height, VkFormat format, 
                         VkImageUsageFlags usage, u32 mip_levels = 1);
    void DestroyImage(GPUImage& image);
    
    // Staging/upload
    void UploadToBuffer(GPUBuffer& buffer, const void* data, VkDeviceSize size);
    void UploadToImage(GPUImage& image, const void* data, VkDeviceSize size);
    
private:
    VulkanContext* m_context = nullptr;
    VkCommandPool m_transfer_pool = VK_NULL_HANDLE;
};

} // namespace action
