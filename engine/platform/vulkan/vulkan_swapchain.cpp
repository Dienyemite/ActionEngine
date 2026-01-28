#include "vulkan_swapchain.h"
#include "vulkan_context.h"
#include "core/logging.h"
#include <algorithm>
#include <limits>

namespace action {

bool VulkanSwapchain::Initialize(VulkanContext& context, const SwapchainConfig& config) {
    m_vsync = config.vsync;
    
    if (!CreateSwapchain(context, config)) {
        return false;
    }
    
    if (!CreateImageViews(context.GetDevice())) {
        return false;
    }
    
    if (!CreateDepthResources(context)) {
        return false;
    }
    
    LOG_INFO("Swapchain created: {}x{}, {} images, format {}", 
             m_extent.width, m_extent.height, 
             m_images.size(), static_cast<int>(m_format));
    
    return true;
}

void VulkanSwapchain::Shutdown(VulkanContext& context) {
    Cleanup(context.GetDevice());
}

bool VulkanSwapchain::Recreate(VulkanContext& context, u32 width, u32 height) {
    context.WaitIdle();
    Cleanup(context.GetDevice());
    
    SwapchainConfig config{
        .width = width,
        .height = height,
        .vsync = m_vsync,
        .image_count = 3
    };
    
    return Initialize(context, config);
}

VkResult VulkanSwapchain::AcquireNextImage(VkDevice device, VkSemaphore semaphore, u32& image_index) {
    return vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX, 
                                  semaphore, VK_NULL_HANDLE, &image_index);
}

VkResult VulkanSwapchain::Present(VkQueue queue, VkSemaphore wait_semaphore, u32 image_index) {
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &wait_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &image_index;
    
    return vkQueuePresentKHR(queue, &present_info);
}

bool VulkanSwapchain::CreateSwapchain(VulkanContext& context, const SwapchainConfig& config) {
    VkPhysicalDevice physical_device = context.GetPhysicalDevice();
    VkSurfaceKHR surface = context.GetSurface();
    
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
    
    // Query supported formats
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());
    
    // Query supported present modes
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());
    
    // Choose best options
    VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(formats);
    VkPresentModeKHR present_mode = ChoosePresentMode(present_modes, config.vsync);
    VkExtent2D extent = ChooseExtent(capabilities, config.width, config.height);
    
    // Image count (triple buffering)
    u32 image_count = config.image_count;
    if (image_count < capabilities.minImageCount) {
        image_count = capabilities.minImageCount;
    }
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    const auto& queue_families = context.GetQueueFamilies();
    u32 queue_family_indices[] = {
        queue_families.graphics.value(),
        queue_families.present.value()
    };
    
    if (queue_families.graphics != queue_families.present) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    
    VkResult result = vkCreateSwapchainKHR(context.GetDevice(), &create_info, nullptr, &m_swapchain);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain: {}", static_cast<int>(result));
        return false;
    }
    
    m_format = surface_format.format;
    m_extent = extent;
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(context.GetDevice(), m_swapchain, &image_count, nullptr);
    m_images.resize(image_count);
    vkGetSwapchainImagesKHR(context.GetDevice(), m_swapchain, &image_count, m_images.data());
    
    return true;
}

bool VulkanSwapchain::CreateImageViews(VkDevice device) {
    m_image_views.resize(m_images.size());
    
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        VkResult result = vkCreateImageView(device, &view_info, nullptr, &m_image_views[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create image view {}: {}", i, static_cast<int>(result));
            return false;
        }
    }
    
    return true;
}

bool VulkanSwapchain::CreateDepthResources(VulkanContext& context) {
    m_depth_format = FindDepthFormat(context.GetPhysicalDevice());
    
    // Create depth image
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = m_extent.width;
    image_info.extent.height = m_extent.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = m_depth_format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = vkCreateImage(context.GetDevice(), &image_info, nullptr, &m_depth_image);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create depth image: {}", static_cast<int>(result));
        return false;
    }
    
    // Allocate memory
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(context.GetDevice(), m_depth_image, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = context.FindMemoryType(
        mem_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    result = vkAllocateMemory(context.GetDevice(), &alloc_info, nullptr, &m_depth_memory);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate depth image memory: {}", static_cast<int>(result));
        return false;
    }
    
    vkBindImageMemory(context.GetDevice(), m_depth_image, m_depth_memory, 0);
    
    // Create image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_depth_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = m_depth_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    result = vkCreateImageView(context.GetDevice(), &view_info, nullptr, &m_depth_image_view);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create depth image view: {}", static_cast<int>(result));
        return false;
    }
    
    return true;
}

VkSurfaceFormatKHR VulkanSwapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    // Prefer SRGB
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && 
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    
    // Fallback to first available
    return formats[0];
}

VkPresentModeKHR VulkanSwapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync) {
    if (!vsync) {
        // Prefer mailbox (triple buffering without tearing)
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }
        
        // Immediate (may tear)
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return mode;
            }
        }
    }
    
    // FIFO is always available (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) {
    if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
        return capabilities.currentExtent;
    }
    
    VkExtent2D extent = {width, height};
    extent.width = std::clamp(extent.width, 
                               capabilities.minImageExtent.width,
                               capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                                capabilities.minImageExtent.height,
                                capabilities.maxImageExtent.height);
    
    return extent;
}

VkFormat VulkanSwapchain::FindDepthFormat(VkPhysicalDevice device) {
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device, format, &props);
        
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    
    LOG_ERROR("Failed to find suitable depth format");
    return VK_FORMAT_D32_SFLOAT;
}

void VulkanSwapchain::Cleanup(VkDevice device) {
    if (m_depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_depth_image_view, nullptr);
        m_depth_image_view = VK_NULL_HANDLE;
    }
    
    if (m_depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_depth_image, nullptr);
        m_depth_image = VK_NULL_HANDLE;
    }
    
    if (m_depth_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_depth_memory, nullptr);
        m_depth_memory = VK_NULL_HANDLE;
    }
    
    for (auto view : m_image_views) {
        vkDestroyImageView(device, view, nullptr);
    }
    m_image_views.clear();
    m_images.clear();
    
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

} // namespace action
