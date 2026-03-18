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
        // HOST_VISIBLE: write directly
        memcpy(buffer.mapped, data, size);
        return;
    }

    // DEVICE_LOCAL: go through a transient staging buffer
    GPUBuffer staging = CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging.buffer) {
        LOG_ERROR("UploadToBuffer: failed to create staging buffer");
        return;
    }
    memcpy(staging.mapped, data, size);

    VkDevice device = m_context->GetDevice();

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool        = m_transfer_pool;
    alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer, buffer.buffer, 1, &region);

    vkEndCommandBuffer(cmd);

    VkQueue queue = m_context->GetTransferQueue();
    if (queue == VK_NULL_HANDLE) queue = m_context->GetGraphicsQueue();

    VkSubmitInfo submit_info{};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &cmd;
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, m_transfer_pool, 1, &cmd);
    DestroyBuffer(staging);
}

void VulkanDevice::UploadToImage(GPUImage& image, const void* data, VkDeviceSize size) {
    if (!image.image || !data || size == 0) return;

    VkDevice device = m_context->GetDevice();

    // Create staging buffer
    GPUBuffer staging = CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging.buffer) {
        LOG_ERROR("UploadToImage: failed to create staging buffer");
        return;
    }
    memcpy(staging.mapped, data, size);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool        = m_transfer_pool;
    alloc_info.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    // UNDEFINED -> TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image.image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = image.mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copy_region{};
    copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel       = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount     = 1;
    copy_region.imageExtent                     = { image.width, image.height, 1 };
    vkCmdCopyBufferToImage(cmd, staging.buffer, image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    // TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkQueue queue = m_context->GetTransferQueue();
    if (queue == VK_NULL_HANDLE) queue = m_context->GetGraphicsQueue();

    VkSubmitInfo submit_info{};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &cmd;
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, m_transfer_pool, 1, &cmd);
    DestroyBuffer(staging);
}

} // namespace action
