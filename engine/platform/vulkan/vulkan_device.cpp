#include "vulkan_device.h"
#include "vulkan_context.h"
#include "core/logging.h"

namespace action {

bool VulkanDevice::Initialize(VulkanContext& context) {
    m_context = &context;
    
    // Create transfer command pool
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = context.GetQueueFamilies().transfer.value_or(
        context.GetQueueFamilies().graphics.value());
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    
    VkResult result = vkCreateCommandPool(context.GetDevice(), &pool_info, nullptr, &m_transfer_pool);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create transfer command pool");
        return false;
    }
    
    return true;
}

void VulkanDevice::Shutdown() {
    if (m_context && m_transfer_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_context->GetDevice(), m_transfer_pool, nullptr);
        m_transfer_pool = VK_NULL_HANDLE;
    }
}

GPUBuffer VulkanDevice::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties) {
    GPUBuffer buffer;
    buffer.size = size;
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkDevice device = m_context->GetDevice();
    
    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer.buffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create buffer");
        return buffer;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = m_context->FindMemoryType(mem_requirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer.memory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate buffer memory");
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
        return buffer;
    }
    
    vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);
    
    // Map if host visible
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        vkMapMemory(device, buffer.memory, 0, size, 0, &buffer.mapped);
    }
    
    return buffer;
}

void VulkanDevice::DestroyBuffer(GPUBuffer& buffer) {
    VkDevice device = m_context->GetDevice();
    
    if (buffer.mapped) {
        vkUnmapMemory(device, buffer.memory);
        buffer.mapped = nullptr;
    }
    
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }
    
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }
}

GPUImage VulkanDevice::CreateImage(u32 width, u32 height, VkFormat format,
                                    VkImageUsageFlags usage, u32 mip_levels) {
    GPUImage image;
    image.width = width;
    image.height = height;
    image.format = format;
    image.mip_levels = mip_levels;
    
    VkDevice device = m_context->GetDevice();
    
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device, &image_info, nullptr, &image.image) != VK_SUCCESS) {
        LOG_ERROR("Failed to create image");
        return image;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device, image.image, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = m_context->FindMemoryType(
        mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(device, &alloc_info, nullptr, &image.memory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate image memory");
        vkDestroyImage(device, image.image, nullptr);
        image.image = VK_NULL_HANDLE;
        return image;
    }
    
    vkBindImageMemory(device, image.image, image.memory, 0);
    
    return image;
}

void VulkanDevice::DestroyImage(GPUImage& image) {
    VkDevice device = m_context->GetDevice();
    
    if (image.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, image.sampler, nullptr);
        image.sampler = VK_NULL_HANDLE;
    }
    
    if (image.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }
    
    if (image.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image.image, nullptr);
        image.image = VK_NULL_HANDLE;
    }
    
    if (image.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image.memory, nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}

void VulkanDevice::UploadToBuffer(GPUBuffer& buffer, const void* data, VkDeviceSize size) {
    if (buffer.mapped) {
        memcpy(buffer.mapped, data, size);
    }
    // TODO: Staging buffer for device-local memory
}

void VulkanDevice::UploadToImage(GPUImage& image, const void* data, VkDeviceSize size) {
    (void)image;
    (void)data;
    (void)size;
    // TODO: Implement staging buffer upload
}

} // namespace action
