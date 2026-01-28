#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace action {

/*
 * Vulkan Context - Core Vulkan initialization
 * 
 * Target: Vulkan 1.0 for GTX 660 (Kepler) compatibility
 * 
 * Features:
 * - Instance creation with validation layers (debug)
 * - Physical device selection (discrete GPU preferred)
 * - Logical device with required queues
 */

struct QueueFamilyIndices {
    std::optional<u32> graphics;
    std::optional<u32> present;
    std::optional<u32> compute;
    std::optional<u32> transfer;
    
    bool IsComplete() const {
        return graphics.has_value() && present.has_value();
    }
};

struct VulkanContextConfig {
    void* window_handle;      // HWND on Windows
    void* instance_handle;    // HINSTANCE on Windows
    u32 width;
    u32 height;
    bool enable_validation;
    bool vsync;
};

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;
    
    bool Initialize(const VulkanContextConfig& config);
    void Shutdown();
    
    // Accessors
    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
    VkDevice GetDevice() const { return m_device; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    
    const QueueFamilyIndices& GetQueueFamilies() const { return m_queue_families; }
    VkQueue GetGraphicsQueue() const { return m_graphics_queue; }
    VkQueue GetPresentQueue() const { return m_present_queue; }
    VkQueue GetComputeQueue() const { return m_compute_queue; }
    VkQueue GetTransferQueue() const { return m_transfer_queue; }
    
    // Device properties
    const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_device_properties; }
    const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_memory_properties; }
    const VkPhysicalDeviceLimits& GetLimits() const { return m_device_properties.limits; }
    
    // Memory allocation helper
    u32 FindMemoryType(u32 type_filter, VkMemoryPropertyFlags properties) const;
    
    // Wait for device idle
    void WaitIdle() const;
    
private:
    bool CreateInstance(bool enable_validation);
    bool CreateSurface(void* window_handle, void* instance_handle);
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    int RateDeviceSuitability(VkPhysicalDevice device);
    
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    
    QueueFamilyIndices m_queue_families;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;
    VkQueue m_compute_queue = VK_NULL_HANDLE;
    VkQueue m_transfer_queue = VK_NULL_HANDLE;
    
    VkPhysicalDeviceProperties m_device_properties{};
    VkPhysicalDeviceMemoryProperties m_memory_properties{};
    VkPhysicalDeviceFeatures m_device_features{};
    
    bool m_validation_enabled = false;
    
    // Required device extensions
    const std::vector<const char*> m_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

// Validation layer callback
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

} // namespace action
