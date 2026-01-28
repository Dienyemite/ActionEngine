#include "imgui_renderer.h"
#include "core/logging.h"
#include "render/renderer.h"
#include "platform/platform.h"
#include "platform/vulkan/vulkan_context.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_win32.h>

namespace action {

bool ImGuiRenderer::Initialize(VulkanContext& context, Renderer& renderer, Platform& platform) {
    m_context = &context;
    
    LOG_INFO("Initializing ImGui renderer...");
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable docking
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Multi-viewport (optional)
    
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = false;  // Dock without holding shift
    
    // Set font size
    io.FontGlobalScale = 1.0f;
    io.Fonts->AddFontDefault();
    
    // Create descriptor pool for ImGui
    if (!CreateDescriptorPool()) {
        LOG_ERROR("Failed to create ImGui descriptor pool");
        return false;
    }
    
    // Initialize platform backend (Win32)
    if (!ImGui_ImplWin32_Init(platform.GetWindowHandle())) {
        LOG_ERROR("Failed to initialize ImGui Win32 backend");
        return false;
    }
    
    // Initialize Vulkan backend
    if (!InitImGuiVulkan(renderer)) {
        LOG_ERROR("Failed to initialize ImGui Vulkan backend");
        return false;
    }
    
    // Upload fonts
    if (!UploadFonts()) {
        LOG_ERROR("Failed to upload ImGui fonts");
        return false;
    }
    
    m_initialized = true;
    LOG_INFO("ImGui renderer initialized");
    return true;
}

void ImGuiRenderer::Shutdown() {
    if (!m_initialized) return;
    
    LOG_INFO("Shutting down ImGui renderer...");
    
    m_context->WaitIdle();
    
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    if (m_imgui_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context->GetDevice(), m_imgui_pool, nullptr);
        m_imgui_pool = VK_NULL_HANDLE;
    }
    
    m_initialized = false;
    LOG_INFO("ImGui renderer shutdown complete");
}

bool ImGuiRenderer::CreateDescriptorPool() {
    // Create a descriptor pool for ImGui
    // This pool size is generous to allow for ImGui's internal usage
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<u32>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(m_context->GetDevice(), &pool_info, nullptr, &m_imgui_pool) != VK_SUCCESS) {
        return false;
    }
    
    return true;
}

bool ImGuiRenderer::InitImGuiVulkan(Renderer& renderer) {
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = m_context->GetInstance();
    init_info.PhysicalDevice = m_context->GetPhysicalDevice();
    init_info.Device = m_context->GetDevice();
    init_info.QueueFamily = m_context->GetQueueFamilies().graphics.value();
    init_info.Queue = m_context->GetGraphicsQueue();
    init_info.DescriptorPool = m_imgui_pool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 3;  // Triple buffering
    init_info.PipelineInfoMain.RenderPass = renderer.GetForwardPass();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        return false;
    }
    
    return true;
}

bool ImGuiRenderer::UploadFonts() {
    // In modern ImGui, fonts are uploaded automatically on first use
    // through the renderer backend, no explicit call needed
    return true;
}

void ImGuiRenderer::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::Render(VkCommandBuffer cmd) {
    // Finalize ImGui frame
    ImGui::Render();
    
    // Render ImGui draw data
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
}

void ImGuiRenderer::EndFrame() {
    // Handle viewports (if enabled)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiRenderer::OnResize(u32 width, u32 height) {
    // ImGui handles resize automatically through Win32 backend
    (void)width;
    (void)height;
}

} // namespace action
