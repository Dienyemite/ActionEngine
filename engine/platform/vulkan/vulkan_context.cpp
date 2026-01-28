#include "vulkan_context.h"
#include "core/logging.h"

#ifdef PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#endif

#include <set>
#include <algorithm>

namespace action {

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) 
{
    (void)type;
    (void)user_data;
    
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_ERROR("[Vulkan] {}", callback_data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("[Vulkan] {}", callback_data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOG_DEBUG("[Vulkan] {}", callback_data->pMessage);
    }
    
    return VK_FALSE;
}

bool VulkanContext::Initialize(const VulkanContextConfig& config) {
    LOG_INFO("Initializing Vulkan context...");
    
    if (!CreateInstance(config.enable_validation)) {
        return false;
    }
    
    if (!CreateSurface(config.window_handle, config.instance_handle)) {
        return false;
    }
    
    if (!SelectPhysicalDevice()) {
        return false;
    }
    
    if (!CreateLogicalDevice()) {
        return false;
    }
    
    LOG_INFO("Vulkan context initialized successfully");
    LOG_INFO("  Device: {}", m_device_properties.deviceName);
    LOG_INFO("  API Version: {}.{}.{}", 
             VK_VERSION_MAJOR(m_device_properties.apiVersion),
             VK_VERSION_MINOR(m_device_properties.apiVersion),
             VK_VERSION_PATCH(m_device_properties.apiVersion));
    LOG_INFO("  VRAM: {} MB", 
             m_memory_properties.memoryHeaps[0].size / (1024 * 1024));
    
    return true;
}

void VulkanContext::Shutdown() {
    LOG_INFO("Shutting down Vulkan context...");
    
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    
    if (m_validation_enabled && m_debug_messenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(m_instance, m_debug_messenger, nullptr);
        }
    }
    
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

bool VulkanContext::CreateInstance(bool enable_validation) {
    m_validation_enabled = enable_validation;
    
    // Check validation layer support
    std::vector<const char*> validation_layers;
    if (enable_validation) {
        u32 layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
        
        const char* requested_layer = "VK_LAYER_KHRONOS_validation";
        bool found = false;
        for (const auto& layer : available_layers) {
            if (strcmp(layer.layerName, requested_layer) == 0) {
                found = true;
                break;
            }
        }
        
        if (found) {
            validation_layers.push_back(requested_layer);
            LOG_INFO("Vulkan validation layers enabled");
        } else {
            LOG_WARN("Vulkan validation layers requested but not available");
        }
    }
    
    // Instance extensions
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef PLATFORM_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
    };
    
    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    // Application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "ActionEngine Game";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "ActionEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;  // Target Vulkan 1.0 for GTX 660
    
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();
    
    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan instance: {}", static_cast<int>(result));
        return false;
    }
    
    // Create debug messenger
    if (enable_validation && !validation_layers.empty()) {
        VkDebugUtilsMessengerCreateInfoEXT debug_info{};
        debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_info.messageSeverity = 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_info.messageType = 
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_info.pfnUserCallback = VulkanDebugCallback;
        
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func) {
            func(m_instance, &debug_info, nullptr, &m_debug_messenger);
        }
    }
    
    return true;
}

bool VulkanContext::CreateSurface(void* window_handle, void* instance_handle) {
#ifdef PLATFORM_WINDOWS
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hwnd = static_cast<HWND>(window_handle);
    create_info.hinstance = static_cast<HINSTANCE>(instance_handle);
    
    VkResult result = vkCreateWin32SurfaceKHR(m_instance, &create_info, nullptr, &m_surface);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan surface: {}", static_cast<int>(result));
        return false;
    }
#else
    (void)window_handle;
    (void)instance_handle;
    LOG_ERROR("Platform not supported");
    return false;
#endif
    
    return true;
}

bool VulkanContext::SelectPhysicalDevice() {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    
    if (device_count == 0) {
        LOG_ERROR("No Vulkan-capable GPUs found");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());
    
    // Score and select best device
    int best_score = -1;
    for (const auto& device : devices) {
        int score = RateDeviceSuitability(device);
        if (score > best_score) {
            m_physical_device = device;
            best_score = score;
        }
    }
    
    if (m_physical_device == VK_NULL_HANDLE) {
        LOG_ERROR("No suitable GPU found");
        return false;
    }
    
    // Get device properties
    vkGetPhysicalDeviceProperties(m_physical_device, &m_device_properties);
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);
    vkGetPhysicalDeviceFeatures(m_physical_device, &m_device_features);
    
    m_queue_families = FindQueueFamilies(m_physical_device);
    
    return true;
}

int VulkanContext::RateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);
    
    // Check queue families
    QueueFamilyIndices indices = FindQueueFamilies(device);
    if (!indices.IsComplete()) {
        return -1;
    }
    
    // Check extension support
    if (!CheckDeviceExtensionSupport(device)) {
        return -1;
    }
    
    int score = 0;
    
    // Prefer discrete GPUs
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    
    // Score based on max texture size
    score += properties.limits.maxImageDimension2D;
    
    // Score based on VRAM
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(device, &mem_props);
    for (u32 i = 0; i < mem_props.memoryHeapCount; ++i) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            score += static_cast<int>(mem_props.memoryHeaps[i].size / (1024 * 1024));
            break;
        }
    }
    
    LOG_DEBUG("GPU: {} - Score: {}", properties.deviceName, score);
    
    return score;
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
    
    for (u32 i = 0; i < queue_family_count; ++i) {
        const auto& family = queue_families[i];
        
        // Graphics queue
        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        
        // Present support
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);
        if (present_support) {
            indices.present = i;
        }
        
        // Compute queue (prefer dedicated)
        if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) && 
            !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.compute = i;
        }
        
        // Transfer queue (prefer dedicated)
        if ((family.queueFlags & VK_QUEUE_TRANSFER_BIT) && 
            !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.transfer = i;
        }
    }
    
    // Fallback: use graphics queue for compute/transfer if no dedicated
    if (!indices.compute.has_value() && indices.graphics.has_value()) {
        indices.compute = indices.graphics;
    }
    if (!indices.transfer.has_value() && indices.graphics.has_value()) {
        indices.transfer = indices.graphics;
    }
    
    return indices;
}

bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());
    
    std::set<std::string> required(m_device_extensions.begin(), m_device_extensions.end());
    
    for (const auto& extension : available_extensions) {
        required.erase(extension.extensionName);
    }
    
    return required.empty();
}

bool VulkanContext::CreateLogicalDevice() {
    // Unique queue families
    std::set<u32> unique_families = {
        m_queue_families.graphics.value(),
        m_queue_families.present.value()
    };
    if (m_queue_families.compute.has_value()) {
        unique_families.insert(m_queue_families.compute.value());
    }
    if (m_queue_families.transfer.has_value()) {
        unique_families.insert(m_queue_families.transfer.value());
    }
    
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;
    
    for (u32 family : unique_families) {
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_info);
    }
    
    // Device features (minimal for GTX 660 compatibility)
    VkPhysicalDeviceFeatures enabled_features{};
    enabled_features.samplerAnisotropy = VK_TRUE;
    enabled_features.fillModeNonSolid = VK_TRUE;  // Wireframe debug
    enabled_features.multiDrawIndirect = VK_TRUE; // For GPU-driven rendering
    
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &enabled_features;
    create_info.enabledExtensionCount = static_cast<u32>(m_device_extensions.size());
    create_info.ppEnabledExtensionNames = m_device_extensions.data();
    
    VkResult result = vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create logical device: {}", static_cast<int>(result));
        return false;
    }
    
    // Get queue handles
    vkGetDeviceQueue(m_device, m_queue_families.graphics.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_queue_families.present.value(), 0, &m_present_queue);
    
    if (m_queue_families.compute.has_value()) {
        vkGetDeviceQueue(m_device, m_queue_families.compute.value(), 0, &m_compute_queue);
    }
    if (m_queue_families.transfer.has_value()) {
        vkGetDeviceQueue(m_device, m_queue_families.transfer.value(), 0, &m_transfer_queue);
    }
    
    return true;
}

u32 VulkanContext::FindMemoryType(u32 type_filter, VkMemoryPropertyFlags properties) const {
    for (u32 i = 0; i < m_memory_properties.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && 
            (m_memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    LOG_ERROR("Failed to find suitable memory type");
    return UINT32_MAX;
}

void VulkanContext::WaitIdle() const {
    vkDeviceWaitIdle(m_device);
}

} // namespace action
