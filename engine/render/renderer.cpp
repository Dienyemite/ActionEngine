#include "renderer.h"
#include "core/logging.h"
#include "core/profiler.h"
#include "assets/asset_manager.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <array>
#include <vector>

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace action {

// GPU data structures (must match shader layouts)
struct CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 cameraPos;
    float time;
};

// Lighting UBO - must match std140 layout in shader
// In std140, vec3 aligns to 16 bytes, so we use vec4 or explicit padding
struct LightingUBO {
    vec4 sunDirection;      // xyz = direction, w = intensity
    vec4 sunColor;          // xyz = color, w = ambient intensity
    vec4 ambientColor;      // xyz = ambient color, w = padding
};

struct PushConstants {
    mat4 model;
    mat4 normalMatrix;
    vec4 color;
};

mat4 Camera::GetViewMatrix() const {
    return mat4::look_at(position, position + forward, up);
}

mat4 Camera::GetProjectionMatrix() const {
    return mat4::perspective(fov, aspect, near_plane, far_plane);
}

mat4 Camera::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

Frustum Camera::GetFrustum() const {
    return Frustum::from_view_proj(GetViewProjectionMatrix());
}

bool Renderer::Initialize(const RendererConfig& config) {
    m_config = config;
    
    LOG_INFO("Initializing renderer...");
    LOG_INFO("  Resolution: {}x{}", config.width, config.height);
    LOG_INFO("  VRAM Budget: {} MB", config.vram_budget / (1024 * 1024));
    LOG_INFO("  Max Draw Calls: {}", config.max_draw_calls);
    
    // Initialize Vulkan context
    VulkanContextConfig ctx_config{
        .window_handle = config.window,
        .instance_handle = nullptr,  // Will be set from platform
        .width = config.width,
        .height = config.height,
        .enable_validation = true,  // TODO: Make configurable
        .vsync = config.vsync
    };
    
#ifdef PLATFORM_WINDOWS
    ctx_config.instance_handle = GetModuleHandle(nullptr);
#endif
    
    if (!m_context.Initialize(ctx_config)) {
        LOG_ERROR("Failed to initialize Vulkan context");
        return false;
    }
    
    // Create swapchain
    SwapchainConfig swap_config{
        .width = config.width,
        .height = config.height,
        .vsync = config.vsync,
        .image_count = 3  // Triple buffering
    };
    
    if (!m_swapchain.Initialize(m_context, swap_config)) {
        LOG_ERROR("Failed to initialize swapchain");
        return false;
    }
    
    // Create render resources
    if (!CreateRenderPasses()) {
        return false;
    }
    
    if (!CreateCommandBuffers()) {
        return false;
    }
    
    if (!CreateSyncObjects()) {
        return false;
    }
    
    if (!CreateDescriptorSets()) {
        return false;
    }
    
    // Create FXAA resources (needs descriptor pool to be created first)
    if (!CreateFXAAResources()) {
        return false;
    }
    
    if (!CreateFramebuffers()) {
        return false;
    }
    
    if (!CreateUniformBuffers()) {
        return false;
    }
    
    if (!CreatePipelines()) {
        return false;
    }
    
    if (!CreateTestMesh()) {
        return false;
    }
    
    // Initialize camera
    m_camera.position = {0, 2, 5};
    m_camera.forward = {0, 0, -1};
    m_camera.up = {0, 1, 0};
    m_camera.aspect = static_cast<float>(config.width) / config.height;
    
    LOG_INFO("Renderer initialized successfully");
    return true;
}

void Renderer::Shutdown() {
    m_context.WaitIdle();
    
    VkDevice device = m_context.GetDevice();
    
    // Destroy sync objects
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_image_available[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_image_available[i], nullptr);
        }
        if (m_render_finished[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_render_finished[i], nullptr);
        }
        if (m_in_flight_fences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device, m_in_flight_fences[i], nullptr);
        }
    }
    
    // Destroy per-image semaphores
    for (auto sem : m_image_render_finished) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_image_render_finished.clear();
    
    // Clear images-in-flight (no need to destroy, they're just tracking references)
    m_images_in_flight.clear();
    
    // Destroy command pool
    if (m_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_command_pool, nullptr);
    }
    
    // Destroy framebuffers
    for (auto fb : m_scene_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    for (auto fb : m_fxaa_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    
    // Destroy FXAA resources
    DestroyFXAAResources();
    
    // Destroy pipelines
    if (m_fxaa_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_fxaa_pipeline, nullptr);
    }
    if (m_fxaa_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_fxaa_pipeline_layout, nullptr);
    }
    if (m_grid_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_grid_pipeline, nullptr);
    }
    if (m_skybox_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_skybox_pipeline, nullptr);
    }
    if (m_forward_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_forward_pipeline, nullptr);
    }
    if (m_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
    }
    
    // Destroy descriptors
    if (m_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    }
    if (m_global_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_global_set_layout, nullptr);
    }
    
    // Destroy uniform buffers
    for (auto& ub : m_uniform_buffers) {
        if (ub.camera_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, ub.camera_buffer, nullptr);
            vkFreeMemory(device, ub.camera_memory, nullptr);
        }
        if (ub.lighting_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, ub.lighting_buffer, nullptr);
            vkFreeMemory(device, ub.lighting_memory, nullptr);
        }
    }
    
    // Destroy test mesh
    if (m_test_vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_test_vertex_buffer, nullptr);
        vkFreeMemory(device, m_test_vertex_memory, nullptr);
    }
    if (m_test_index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_test_index_buffer, nullptr);
        vkFreeMemory(device, m_test_index_memory, nullptr);
    }
    
    // Destroy render passes
    if (m_depth_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_depth_pass, nullptr);
    }
    if (m_forward_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_forward_pass, nullptr);
    }
    if (m_fxaa_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_fxaa_pass, nullptr);
    }
    if (m_fxaa_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_fxaa_set_layout, nullptr);
    }
    
    m_swapchain.Shutdown(m_context);
    m_context.Shutdown();
    
    LOG_INFO("Renderer shutdown complete");
}

void Renderer::BeginFrame() {
    PROFILE_SCOPE("Renderer::BeginFrame");
    
    // Reset stats
    m_stats.draw_calls = 0;
    m_stats.triangles = 0;
    
    // Note: Fence wait moved to RenderScene to properly synchronize with swapchain
}

void Renderer::RenderScene(const RenderList& render_list) {
    PROFILE_SCOPE("Renderer::RenderScene");
    
    // Debug: Log render list stats periodically
    static u32 frame_counter = 0;
    if (++frame_counter == 60) {  // Every 60 frames (~1 second)
        frame_counter = 0;
        LOG_INFO("RenderList: {} opaque objects, {} transparent, {} draw calls",
                 render_list.opaque.size(), render_list.transparent.size(), 
                 render_list.total_draw_calls);
    }
    
    VkDevice device = m_context.GetDevice();
    
    // Wait for previous frame using this slot BEFORE acquiring
    // This ensures the semaphore from the previous use of this frame slot is no longer in use
    vkWaitForFences(device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);
    
    // Acquire swapchain image
    u32 image_index;
    VkResult result = m_swapchain.AcquireNextImage(device, 
                                                    m_image_available[m_current_frame], 
                                                    image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        OnResize(m_config.width, m_config.height);
        return;
    }
    
    // Wait for any previous frame that was using this swapchain image
    // This is the key synchronization - we need to wait until the image is no longer in use
    if (m_images_in_flight[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &m_images_in_flight[image_index], VK_TRUE, UINT64_MAX);
    }
    // Mark this image as now in use by this frame
    m_images_in_flight[image_index] = m_in_flight_fences[m_current_frame];
    
    vkResetFences(device, 1, &m_in_flight_fences[m_current_frame]);
    
    // Record command buffer
    vkResetCommandBuffer(m_command_buffers[m_current_frame], 0);
    RecordCommandBuffer(image_index, render_list);
    
    // Submit command buffer
    // Use per-image semaphore for signaling - this prevents semaphore reuse issues
    // because each swapchain image has its own semaphore that is only signaled
    // when that specific image's rendering is complete
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore wait_semaphores[] = {m_image_available[m_current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[m_current_frame];
    
    // Signal the per-image semaphore (indexed by acquired image, not frame-in-flight)
    VkSemaphore signal_semaphores[] = {m_image_render_finished[image_index]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    
    vkQueueSubmit(m_context.GetGraphicsQueue(), 1, &submit_info, 
                  m_in_flight_fences[m_current_frame]);
    
    // Present using the per-image semaphore
    result = m_swapchain.Present(m_context.GetPresentQueue(), 
                                  m_image_render_finished[image_index], 
                                  image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        OnResize(m_config.width, m_config.height);
    }
    
    // Update stats
    m_stats.draw_calls = render_list.total_draw_calls;
    m_stats.triangles = render_list.total_triangles;
}

void Renderer::EndFrame() {
    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::SetCamera(const Camera& camera) {
    m_camera = camera;
}

void Renderer::SetLighting(const LightingData& lighting) {
    m_lighting = lighting;
}

void Renderer::OnResize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    
    m_config.width = width;
    m_config.height = height;
    m_camera.aspect = static_cast<float>(width) / height;
    
    m_context.WaitIdle();
    
    VkDevice device = m_context.GetDevice();
    
    // Destroy old framebuffers
    for (auto fb : m_scene_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    m_scene_framebuffers.clear();
    
    for (auto fb : m_fxaa_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    m_fxaa_framebuffers.clear();
    
    // Destroy FXAA resources (they're resolution-dependent)
    DestroyFXAAResources();
    
    // Destroy old per-image semaphores (image count might change)
    for (auto sem : m_image_render_finished) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_image_render_finished.clear();
    
    // Recreate swapchain
    m_swapchain.Recreate(m_context, width, height);
    
    // Recreate FXAA resources with new resolution
    CreateFXAAResources();
    
    // Recreate per-image semaphores for new swapchain
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    u32 image_count = m_swapchain.GetImageCount();
    m_image_render_finished.resize(image_count);
    for (u32 i = 0; i < image_count; ++i) {
        vkCreateSemaphore(device, &semaphore_info, nullptr, &m_image_render_finished[i]);
    }
    m_images_in_flight.resize(image_count, VK_NULL_HANDLE);
    
    // Recreate framebuffers
    CreateFramebuffers();
    
    LOG_INFO("Renderer resized to {}x{}", width, height);
}

bool Renderer::CreateRenderPasses() {
    VkDevice device = m_context.GetDevice();
    
    // Forward render pass (renders to offscreen texture for FXAA)
    VkAttachmentDescription color_attachment{};
    color_attachment.format = m_swapchain.GetFormat();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // For FXAA sampling
    
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = m_swapchain.GetDepthFormat();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
    
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<u32>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    
    if (vkCreateRenderPass(device, &render_pass_info, nullptr, &m_forward_pass) != VK_SUCCESS) {
        LOG_ERROR("Failed to create forward render pass");
        return false;
    }
    
    // FXAA render pass (outputs to swapchain)
    VkAttachmentDescription fxaa_color{};
    fxaa_color.format = m_swapchain.GetFormat();
    fxaa_color.samples = VK_SAMPLE_COUNT_1_BIT;
    fxaa_color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // We'll overwrite everything
    fxaa_color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    fxaa_color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    fxaa_color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    fxaa_color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    fxaa_color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference fxaa_color_ref{};
    fxaa_color_ref.attachment = 0;
    fxaa_color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription fxaa_subpass{};
    fxaa_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    fxaa_subpass.colorAttachmentCount = 1;
    fxaa_subpass.pColorAttachments = &fxaa_color_ref;
    
    VkSubpassDependency fxaa_dependency{};
    fxaa_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    fxaa_dependency.dstSubpass = 0;
    fxaa_dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    fxaa_dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    fxaa_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    fxaa_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo fxaa_pass_info{};
    fxaa_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    fxaa_pass_info.attachmentCount = 1;
    fxaa_pass_info.pAttachments = &fxaa_color;
    fxaa_pass_info.subpassCount = 1;
    fxaa_pass_info.pSubpasses = &fxaa_subpass;
    fxaa_pass_info.dependencyCount = 1;
    fxaa_pass_info.pDependencies = &fxaa_dependency;
    
    if (vkCreateRenderPass(device, &fxaa_pass_info, nullptr, &m_fxaa_pass) != VK_SUCCESS) {
        LOG_ERROR("Failed to create FXAA render pass");
        return false;
    }
    
    LOG_INFO("Created render passes (forward + FXAA)");
    return true;
}

bool Renderer::CreateFramebuffers() {
    VkDevice device = m_context.GetDevice();
    VkExtent2D extent = m_swapchain.GetExtent();
    u32 image_count = m_swapchain.GetImageCount();
    
    // Scene framebuffers (render to offscreen texture)
    m_scene_framebuffers.resize(image_count);
    for (size_t i = 0; i < image_count; ++i) {
        std::array<VkImageView, 2> attachments = {
            m_scene_image_view,  // Offscreen render target
            m_swapchain.GetDepthImageView()
        };
        
        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = m_forward_pass;
        fb_info.attachmentCount = static_cast<u32>(attachments.size());
        fb_info.pAttachments = attachments.data();
        fb_info.width = extent.width;
        fb_info.height = extent.height;
        fb_info.layers = 1;
        
        if (vkCreateFramebuffer(device, &fb_info, nullptr, &m_scene_framebuffers[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create scene framebuffer {}", i);
            return false;
        }
    }
    
    // FXAA framebuffers (output to swapchain)
    m_fxaa_framebuffers.resize(image_count);
    for (size_t i = 0; i < m_swapchain.GetImageViews().size(); ++i) {
        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = m_fxaa_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = &m_swapchain.GetImageViews()[i];
        fb_info.width = extent.width;
        fb_info.height = extent.height;
        fb_info.layers = 1;
        
        if (vkCreateFramebuffer(device, &fb_info, nullptr, &m_fxaa_framebuffers[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create FXAA framebuffer {}", i);
            return false;
        }
    }
    
    return true;
}

bool Renderer::CreateCommandBuffers() {
    VkDevice device = m_context.GetDevice();
    
    // Create command pool
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = m_context.GetQueueFamilies().graphics.value();
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create command pool");
        return false;
    }
    
    // Allocate command buffers
    m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<u32>(m_command_buffers.size());
    
    if (vkAllocateCommandBuffers(device, &alloc_info, m_command_buffers.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffers");
        return false;
    }
    
    return true;
}

bool Renderer::CreateSyncObjects() {
    VkDevice device = m_context.GetDevice();
    
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    // Create per-frame-in-flight resources
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device, &semaphore_info, nullptr, &m_image_available[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphore_info, nullptr, &m_render_finished[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fence_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create sync objects for frame {}", i);
            return false;
        }
    }
    
    // Create per-swapchain-image semaphores for present synchronization
    // This fixes the semaphore reuse issue with non-sequential image acquisition
    u32 image_count = m_swapchain.GetImageCount();
    m_image_render_finished.resize(image_count);
    for (u32 i = 0; i < image_count; ++i) {
        if (vkCreateSemaphore(device, &semaphore_info, nullptr, &m_image_render_finished[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create per-image semaphore {}", i);
            return false;
        }
    }
    
    // Initialize images-in-flight tracking
    // Each swapchain image tracks which fence it's associated with
    m_images_in_flight.resize(image_count, VK_NULL_HANDLE);
    
    return true;
}
VkShaderModule Renderer::LoadShaderModule(const std::string& path) {
    // Read SPIR-V file
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", path);
        return VK_NULL_HANDLE;
    }
    
    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();
    
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buffer.size();
    create_info.pCode = reinterpret_cast<const u32*>(buffer.data());
    
    VkShaderModule shader_module;
    if (vkCreateShaderModule(m_context.GetDevice(), &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        LOG_ERROR("Failed to create shader module: {}", path);
        return VK_NULL_HANDLE;
    }
    
    LOG_INFO("Loaded shader: {}", path);
    return shader_module;
}

bool Renderer::CreateDescriptorSets() {
    VkDevice device = m_context.GetDevice();
    
    // Create descriptor set layout for global uniforms (set 0)
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    
    // Binding 0: Camera UBO
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    // Binding 1: Lighting UBO
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<u32>(bindings.size());
    layout_info.pBindings = bindings.data();
    
    if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &m_global_set_layout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor set layout");
        return false;
    }
    
    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 2> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 2 * MAX_FRAMES_IN_FLIGHT;  // Camera + Lighting per frame
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;  // FXAA scene texture per frame
    
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = MAX_FRAMES_IN_FLIGHT * 2;  // Global sets + FXAA sets
    
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor pool");
        return false;
    }
    
    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    layouts.fill(m_global_set_layout);
    
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    alloc_info.pSetLayouts = layouts.data();
    
    if (vkAllocateDescriptorSets(device, &alloc_info, m_global_descriptor_sets.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate descriptor sets");
        return false;
    }
    
    LOG_INFO("Created descriptor sets");
    return true;
}

bool Renderer::CreateUniformBuffers() {
    VkDevice device = m_context.GetDevice();
    VkDeviceSize camera_size = sizeof(CameraUBO);
    VkDeviceSize lighting_size = sizeof(LightingUBO);
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // Camera buffer
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = camera_size;
        buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device, &buffer_info, nullptr, &m_uniform_buffers[i].camera_buffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to create camera uniform buffer");
            return false;
        }
        
        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, m_uniform_buffers[i].camera_buffer, &mem_req);
        
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = m_context.FindMemoryType(mem_req.memoryTypeBits, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(device, &alloc_info, nullptr, &m_uniform_buffers[i].camera_memory) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate camera uniform memory");
            return false;
        }
        
        vkBindBufferMemory(device, m_uniform_buffers[i].camera_buffer, m_uniform_buffers[i].camera_memory, 0);
        vkMapMemory(device, m_uniform_buffers[i].camera_memory, 0, camera_size, 0, &m_uniform_buffers[i].camera_mapped);
        
        // Lighting buffer
        buffer_info.size = lighting_size;
        
        if (vkCreateBuffer(device, &buffer_info, nullptr, &m_uniform_buffers[i].lighting_buffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to create lighting uniform buffer");
            return false;
        }
        
        vkGetBufferMemoryRequirements(device, m_uniform_buffers[i].lighting_buffer, &mem_req);
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = m_context.FindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(device, &alloc_info, nullptr, &m_uniform_buffers[i].lighting_memory) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate lighting uniform memory");
            return false;
        }
        
        vkBindBufferMemory(device, m_uniform_buffers[i].lighting_buffer, m_uniform_buffers[i].lighting_memory, 0);
        vkMapMemory(device, m_uniform_buffers[i].lighting_memory, 0, lighting_size, 0, &m_uniform_buffers[i].lighting_mapped);
        
        // Update descriptor sets
        std::array<VkDescriptorBufferInfo, 2> buffer_infos{};
        buffer_infos[0].buffer = m_uniform_buffers[i].camera_buffer;
        buffer_infos[0].offset = 0;
        buffer_infos[0].range = camera_size;
        
        buffer_infos[1].buffer = m_uniform_buffers[i].lighting_buffer;
        buffer_infos[1].offset = 0;
        buffer_infos[1].range = lighting_size;
        
        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_global_descriptor_sets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &buffer_infos[0];
        
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_global_descriptor_sets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &buffer_infos[1];
        
        vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }
    
    LOG_INFO("Created uniform buffers");
    return true;
}

void Renderer::UpdateUniformBuffers() {
    // Update camera UBO
    CameraUBO camera_ubo{};
    camera_ubo.view = m_camera.GetViewMatrix();
    camera_ubo.projection = m_camera.GetProjectionMatrix();
    camera_ubo.viewProjection = m_camera.GetViewProjectionMatrix();
    camera_ubo.cameraPos = m_camera.position;
    camera_ubo.time = 0.0f;  // TODO: Add time
    
    memcpy(m_uniform_buffers[m_current_frame].camera_mapped, &camera_ubo, sizeof(camera_ubo));
    
    // Update lighting UBO (using vec4 for proper std140 alignment)
    LightingUBO lighting_ubo{};
    lighting_ubo.sunDirection = vec4{m_lighting.sun.direction.x, m_lighting.sun.direction.y, 
                                      m_lighting.sun.direction.z, m_lighting.sun.intensity};
    lighting_ubo.sunColor = vec4{m_lighting.sun.color.x, m_lighting.sun.color.y,
                                  m_lighting.sun.color.z, 0.3f};  // w = ambient intensity
    lighting_ubo.ambientColor = vec4{m_lighting.ambient_color.x, m_lighting.ambient_color.y,
                                      m_lighting.ambient_color.z, 0.0f};
    
    memcpy(m_uniform_buffers[m_current_frame].lighting_mapped, &lighting_ubo, sizeof(lighting_ubo));
}

bool Renderer::CreatePipelines() {
    VkDevice device = m_context.GetDevice();
    
    // Load shaders
    VkShaderModule vert_module = LoadShaderModule("shaders/compiled/forward_vert.spv");
    VkShaderModule frag_module = LoadShaderModule("shaders/compiled/forward_frag.spv");
    
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to load shader modules");
        return false;
    }
    
    // Shader stages
    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";
    
    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vert_stage, frag_stage};
    
    // Vertex input (matches ProceduralVertex: vec3 pos, vec3 normal, vec2 uv)
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(float) * 8;  // 3 + 3 + 2 floats
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::array<VkVertexInputAttributeDescription, 3> attrib_descs{};
    attrib_descs[0].binding = 0;
    attrib_descs[0].location = 0;
    attrib_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_descs[0].offset = 0;
    
    attrib_descs[1].binding = 0;
    attrib_descs[1].location = 1;
    attrib_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_descs[1].offset = sizeof(float) * 3;
    
    attrib_descs[2].binding = 0;
    attrib_descs[2].location = 2;
    attrib_descs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrib_descs[2].offset = sizeof(float) * 6;
    
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = static_cast<u32>(attrib_descs.size());
    vertex_input.pVertexAttributeDescriptions = attrib_descs.data();
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // CCW winding for front faces
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    
    // Dynamic state
    std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<u32>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();
    
    // Push constants
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(PushConstants);
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &m_global_set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    
    if (vkCreatePipelineLayout(device, &layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout");
        vkDestroyShaderModule(device, vert_module, nullptr);
        vkDestroyShaderModule(device, frag_module, nullptr);
        return false;
    }
    
    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<u32>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_forward_pass;
    pipeline_info.subpass = 0;
    
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_forward_pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline");
        vkDestroyShaderModule(device, vert_module, nullptr);
        vkDestroyShaderModule(device, frag_module, nullptr);
        return false;
    }
    
    // Cleanup shader modules (no longer needed after pipeline creation)
    vkDestroyShaderModule(device, vert_module, nullptr);
    vkDestroyShaderModule(device, frag_module, nullptr);
    
    LOG_INFO("Created forward rendering pipeline");
    
    // ========================================
    // Create skybox pipeline
    // ========================================
    VkShaderModule skybox_vert = LoadShaderModule("shaders/compiled/skybox_vert.spv");
    VkShaderModule skybox_frag = LoadShaderModule("shaders/compiled/skybox_frag.spv");
    
    if (skybox_vert == VK_NULL_HANDLE || skybox_frag == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to load skybox shader modules");
        return false;
    }
    
    VkPipelineShaderStageCreateInfo skybox_vert_stage{};
    skybox_vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    skybox_vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    skybox_vert_stage.module = skybox_vert;
    skybox_vert_stage.pName = "main";
    
    VkPipelineShaderStageCreateInfo skybox_frag_stage{};
    skybox_frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    skybox_frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    skybox_frag_stage.module = skybox_frag;
    skybox_frag_stage.pName = "main";
    
    std::array<VkPipelineShaderStageCreateInfo, 2> skybox_stages = {skybox_vert_stage, skybox_frag_stage};
    
    // Skybox has no vertex input (generates fullscreen triangle procedurally)
    VkPipelineVertexInputStateCreateInfo skybox_vertex_input{};
    skybox_vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    skybox_vertex_input.vertexBindingDescriptionCount = 0;
    skybox_vertex_input.vertexAttributeDescriptionCount = 0;
    
    // Same input assembly
    VkPipelineInputAssemblyStateCreateInfo skybox_input_assembly{};
    skybox_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    skybox_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    skybox_input_assembly.primitiveRestartEnable = VK_FALSE;
    
    // Same viewport state (dynamic)
    VkPipelineViewportStateCreateInfo skybox_viewport{};
    skybox_viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    skybox_viewport.viewportCount = 1;
    skybox_viewport.scissorCount = 1;
    
    // Rasterizer - no culling for fullscreen triangle
    VkPipelineRasterizationStateCreateInfo skybox_rasterizer{};
    skybox_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    skybox_rasterizer.depthClampEnable = VK_FALSE;
    skybox_rasterizer.rasterizerDiscardEnable = VK_FALSE;
    skybox_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    skybox_rasterizer.lineWidth = 1.0f;
    skybox_rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling
    skybox_rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    skybox_rasterizer.depthBiasEnable = VK_FALSE;
    
    // Same multisampling
    VkPipelineMultisampleStateCreateInfo skybox_multisampling{};
    skybox_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    skybox_multisampling.sampleShadingEnable = VK_FALSE;
    skybox_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth - test but don't write (skybox is behind everything)
    VkPipelineDepthStencilStateCreateInfo skybox_depth{};
    skybox_depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    skybox_depth.depthTestEnable = VK_FALSE;   // Drawn first, no depth test
    skybox_depth.depthWriteEnable = VK_FALSE;  // Don't write to depth
    skybox_depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    skybox_depth.depthBoundsTestEnable = VK_FALSE;
    skybox_depth.stencilTestEnable = VK_FALSE;
    
    // Same color blending
    VkPipelineColorBlendAttachmentState skybox_blend_attachment{};
    skybox_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    skybox_blend_attachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo skybox_blending{};
    skybox_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    skybox_blending.logicOpEnable = VK_FALSE;
    skybox_blending.attachmentCount = 1;
    skybox_blending.pAttachments = &skybox_blend_attachment;
    
    // Same dynamic state
    std::array<VkDynamicState, 2> skybox_dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo skybox_dynamic{};
    skybox_dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    skybox_dynamic.dynamicStateCount = static_cast<u32>(skybox_dynamic_states.size());
    skybox_dynamic.pDynamicStates = skybox_dynamic_states.data();
    
    // Skybox pipeline (reuses same layout - only needs camera UBO)
    VkGraphicsPipelineCreateInfo skybox_pipeline_info{};
    skybox_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    skybox_pipeline_info.stageCount = static_cast<u32>(skybox_stages.size());
    skybox_pipeline_info.pStages = skybox_stages.data();
    skybox_pipeline_info.pVertexInputState = &skybox_vertex_input;
    skybox_pipeline_info.pInputAssemblyState = &skybox_input_assembly;
    skybox_pipeline_info.pViewportState = &skybox_viewport;
    skybox_pipeline_info.pRasterizationState = &skybox_rasterizer;
    skybox_pipeline_info.pMultisampleState = &skybox_multisampling;
    skybox_pipeline_info.pDepthStencilState = &skybox_depth;
    skybox_pipeline_info.pColorBlendState = &skybox_blending;
    skybox_pipeline_info.pDynamicState = &skybox_dynamic;
    skybox_pipeline_info.layout = m_pipeline_layout;  // Reuse same layout
    skybox_pipeline_info.renderPass = m_forward_pass;
    skybox_pipeline_info.subpass = 0;
    
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skybox_pipeline_info, nullptr, &m_skybox_pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create skybox pipeline");
        vkDestroyShaderModule(device, skybox_vert, nullptr);
        vkDestroyShaderModule(device, skybox_frag, nullptr);
        return false;
    }
    
    vkDestroyShaderModule(device, skybox_vert, nullptr);
    vkDestroyShaderModule(device, skybox_frag, nullptr);
    
    LOG_INFO("Created skybox pipeline");
    
    // ========================================
    // Create grid pipeline (infinite editor grid)
    // ========================================
    VkShaderModule grid_vert = LoadShaderModule("shaders/compiled/grid_vert.spv");
    VkShaderModule grid_frag = LoadShaderModule("shaders/compiled/grid_frag.spv");
    
    if (grid_vert == VK_NULL_HANDLE || grid_frag == VK_NULL_HANDLE) {
        LOG_WARN("Grid shaders not found - grid will be disabled");
        // Grid is optional, don't fail initialization
    } else {
        VkPipelineShaderStageCreateInfo grid_vert_stage{};
        grid_vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        grid_vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        grid_vert_stage.module = grid_vert;
        grid_vert_stage.pName = "main";
        
        VkPipelineShaderStageCreateInfo grid_frag_stage{};
        grid_frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        grid_frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        grid_frag_stage.module = grid_frag;
        grid_frag_stage.pName = "main";
        
        std::array<VkPipelineShaderStageCreateInfo, 2> grid_stages = {grid_vert_stage, grid_frag_stage};
        
        // No vertex input (fullscreen triangle)
        VkPipelineVertexInputStateCreateInfo grid_vertex_input{};
        grid_vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        grid_vertex_input.vertexBindingDescriptionCount = 0;
        grid_vertex_input.vertexAttributeDescriptionCount = 0;
        
        VkPipelineInputAssemblyStateCreateInfo grid_input_assembly{};
        grid_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        grid_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        grid_input_assembly.primitiveRestartEnable = VK_FALSE;
        
        VkPipelineViewportStateCreateInfo grid_viewport{};
        grid_viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        grid_viewport.viewportCount = 1;
        grid_viewport.scissorCount = 1;
        
        VkPipelineRasterizationStateCreateInfo grid_rasterizer{};
        grid_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        grid_rasterizer.depthClampEnable = VK_FALSE;
        grid_rasterizer.rasterizerDiscardEnable = VK_FALSE;
        grid_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        grid_rasterizer.lineWidth = 1.0f;
        grid_rasterizer.cullMode = VK_CULL_MODE_NONE;
        grid_rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        grid_rasterizer.depthBiasEnable = VK_FALSE;
        
        VkPipelineMultisampleStateCreateInfo grid_multisampling{};
        grid_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        grid_multisampling.sampleShadingEnable = VK_FALSE;
        grid_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        // Depth test enabled, write enabled (for proper occlusion)
        VkPipelineDepthStencilStateCreateInfo grid_depth{};
        grid_depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        grid_depth.depthTestEnable = VK_TRUE;
        grid_depth.depthWriteEnable = VK_TRUE;
        grid_depth.depthCompareOp = VK_COMPARE_OP_LESS;
        grid_depth.depthBoundsTestEnable = VK_FALSE;
        grid_depth.stencilTestEnable = VK_FALSE;
        
        // Alpha blending for grid fade
        VkPipelineColorBlendAttachmentState grid_blend_attachment{};
        grid_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        grid_blend_attachment.blendEnable = VK_TRUE;
        grid_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        grid_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        grid_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        grid_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        grid_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        grid_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        
        VkPipelineColorBlendStateCreateInfo grid_blending{};
        grid_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        grid_blending.logicOpEnable = VK_FALSE;
        grid_blending.attachmentCount = 1;
        grid_blending.pAttachments = &grid_blend_attachment;
        
        std::array<VkDynamicState, 2> grid_dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo grid_dynamic{};
        grid_dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        grid_dynamic.dynamicStateCount = static_cast<u32>(grid_dynamic_states.size());
        grid_dynamic.pDynamicStates = grid_dynamic_states.data();
        
        VkGraphicsPipelineCreateInfo grid_pipeline_info{};
        grid_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        grid_pipeline_info.stageCount = static_cast<u32>(grid_stages.size());
        grid_pipeline_info.pStages = grid_stages.data();
        grid_pipeline_info.pVertexInputState = &grid_vertex_input;
        grid_pipeline_info.pInputAssemblyState = &grid_input_assembly;
        grid_pipeline_info.pViewportState = &grid_viewport;
        grid_pipeline_info.pRasterizationState = &grid_rasterizer;
        grid_pipeline_info.pMultisampleState = &grid_multisampling;
        grid_pipeline_info.pDepthStencilState = &grid_depth;
        grid_pipeline_info.pColorBlendState = &grid_blending;
        grid_pipeline_info.pDynamicState = &grid_dynamic;
        grid_pipeline_info.layout = m_pipeline_layout;
        grid_pipeline_info.renderPass = m_forward_pass;
        grid_pipeline_info.subpass = 0;
        
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &grid_pipeline_info, nullptr, &m_grid_pipeline) != VK_SUCCESS) {
            LOG_WARN("Failed to create grid pipeline - grid will be disabled");
        } else {
            LOG_INFO("Created grid pipeline");
        }
        
        vkDestroyShaderModule(device, grid_vert, nullptr);
        vkDestroyShaderModule(device, grid_frag, nullptr);
    }
    
    // ========================================
    // Create FXAA pipeline
    // ========================================
    VkShaderModule fxaa_vert = LoadShaderModule("shaders/compiled/fxaa_vert.spv");
    VkShaderModule fxaa_frag = LoadShaderModule("shaders/compiled/fxaa_frag.spv");
    
    if (fxaa_vert == VK_NULL_HANDLE || fxaa_frag == VK_NULL_HANDLE) {
        LOG_WARN("FXAA shaders not found - anti-aliasing will be disabled");
        m_fxaa_enabled = false;
    } else {
        // Create FXAA pipeline layout (uses set 1 for scene texture)
        VkPushConstantRange fxaa_push_range{};
        fxaa_push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        fxaa_push_range.offset = 0;
        fxaa_push_range.size = sizeof(float) * 4;  // texelSize (vec2) + qualitySubpix + qualityEdgeThreshold
        
        VkPipelineLayoutCreateInfo fxaa_layout_info{};
        fxaa_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        fxaa_layout_info.setLayoutCount = 1;
        fxaa_layout_info.pSetLayouts = &m_fxaa_set_layout;
        fxaa_layout_info.pushConstantRangeCount = 1;
        fxaa_layout_info.pPushConstantRanges = &fxaa_push_range;
        
        if (vkCreatePipelineLayout(device, &fxaa_layout_info, nullptr, &m_fxaa_pipeline_layout) != VK_SUCCESS) {
            LOG_ERROR("Failed to create FXAA pipeline layout");
            vkDestroyShaderModule(device, fxaa_vert, nullptr);
            vkDestroyShaderModule(device, fxaa_frag, nullptr);
            return false;
        }
        
        VkPipelineShaderStageCreateInfo fxaa_vert_stage{};
        fxaa_vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fxaa_vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        fxaa_vert_stage.module = fxaa_vert;
        fxaa_vert_stage.pName = "main";
        
        VkPipelineShaderStageCreateInfo fxaa_frag_stage{};
        fxaa_frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fxaa_frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fxaa_frag_stage.module = fxaa_frag;
        fxaa_frag_stage.pName = "main";
        
        std::array<VkPipelineShaderStageCreateInfo, 2> fxaa_stages = {fxaa_vert_stage, fxaa_frag_stage};
        
        VkPipelineVertexInputStateCreateInfo fxaa_vertex_input{};
        fxaa_vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        fxaa_vertex_input.vertexBindingDescriptionCount = 0;
        fxaa_vertex_input.vertexAttributeDescriptionCount = 0;
        
        VkPipelineInputAssemblyStateCreateInfo fxaa_input_assembly{};
        fxaa_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        fxaa_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        fxaa_input_assembly.primitiveRestartEnable = VK_FALSE;
        
        VkPipelineViewportStateCreateInfo fxaa_viewport{};
        fxaa_viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        fxaa_viewport.viewportCount = 1;
        fxaa_viewport.scissorCount = 1;
        
        VkPipelineRasterizationStateCreateInfo fxaa_rasterizer{};
        fxaa_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        fxaa_rasterizer.depthClampEnable = VK_FALSE;
        fxaa_rasterizer.rasterizerDiscardEnable = VK_FALSE;
        fxaa_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        fxaa_rasterizer.lineWidth = 1.0f;
        fxaa_rasterizer.cullMode = VK_CULL_MODE_NONE;
        fxaa_rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        fxaa_rasterizer.depthBiasEnable = VK_FALSE;
        
        VkPipelineMultisampleStateCreateInfo fxaa_multisampling{};
        fxaa_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        fxaa_multisampling.sampleShadingEnable = VK_FALSE;
        fxaa_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        VkPipelineDepthStencilStateCreateInfo fxaa_depth{};
        fxaa_depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        fxaa_depth.depthTestEnable = VK_FALSE;
        fxaa_depth.depthWriteEnable = VK_FALSE;
        
        VkPipelineColorBlendAttachmentState fxaa_blend_attachment{};
        fxaa_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        fxaa_blend_attachment.blendEnable = VK_FALSE;
        
        VkPipelineColorBlendStateCreateInfo fxaa_blending{};
        fxaa_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        fxaa_blending.logicOpEnable = VK_FALSE;
        fxaa_blending.attachmentCount = 1;
        fxaa_blending.pAttachments = &fxaa_blend_attachment;
        
        std::array<VkDynamicState, 2> fxaa_dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo fxaa_dynamic{};
        fxaa_dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        fxaa_dynamic.dynamicStateCount = static_cast<u32>(fxaa_dynamic_states.size());
        fxaa_dynamic.pDynamicStates = fxaa_dynamic_states.data();
        
        VkGraphicsPipelineCreateInfo fxaa_pipeline_info{};
        fxaa_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        fxaa_pipeline_info.stageCount = static_cast<u32>(fxaa_stages.size());
        fxaa_pipeline_info.pStages = fxaa_stages.data();
        fxaa_pipeline_info.pVertexInputState = &fxaa_vertex_input;
        fxaa_pipeline_info.pInputAssemblyState = &fxaa_input_assembly;
        fxaa_pipeline_info.pViewportState = &fxaa_viewport;
        fxaa_pipeline_info.pRasterizationState = &fxaa_rasterizer;
        fxaa_pipeline_info.pMultisampleState = &fxaa_multisampling;
        fxaa_pipeline_info.pDepthStencilState = &fxaa_depth;
        fxaa_pipeline_info.pColorBlendState = &fxaa_blending;
        fxaa_pipeline_info.pDynamicState = &fxaa_dynamic;
        fxaa_pipeline_info.layout = m_fxaa_pipeline_layout;
        fxaa_pipeline_info.renderPass = m_fxaa_pass;
        fxaa_pipeline_info.subpass = 0;
        
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &fxaa_pipeline_info, nullptr, &m_fxaa_pipeline) != VK_SUCCESS) {
            LOG_WARN("Failed to create FXAA pipeline - anti-aliasing will be disabled");
            m_fxaa_enabled = false;
        } else {
            LOG_INFO("Created FXAA pipeline");
        }
        
        vkDestroyShaderModule(device, fxaa_vert, nullptr);
        vkDestroyShaderModule(device, fxaa_frag, nullptr);
    }
    
    return true;
}

bool Renderer::CreateFXAAResources() {
    VkDevice device = m_context.GetDevice();
    VkExtent2D extent = m_swapchain.GetExtent();
    
    // Only create descriptor layout and allocate descriptor sets on first call
    // (not during resize - descriptor sets persist across resize)
    bool first_time = (m_fxaa_set_layout == VK_NULL_HANDLE);
    
    if (first_time) {
        // Create FXAA descriptor set layout
        VkDescriptorSetLayoutBinding sampler_binding{};
        sampler_binding.binding = 0;
        sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_binding.descriptorCount = 1;
        sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = &sampler_binding;
        
        if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &m_fxaa_set_layout) != VK_SUCCESS) {
            LOG_ERROR("Failed to create FXAA descriptor set layout");
            return false;
        }
        
        // Allocate FXAA descriptor sets from the existing pool
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_fxaa_set_layout);
        VkDescriptorSetAllocateInfo set_alloc_info{};
        set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set_alloc_info.descriptorPool = m_descriptor_pool;
        set_alloc_info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        set_alloc_info.pSetLayouts = layouts.data();
        
        if (vkAllocateDescriptorSets(device, &set_alloc_info, m_fxaa_descriptor_sets.data()) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate FXAA descriptor sets");
            return false;
        }
    }
    
    // Create offscreen scene image
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = m_swapchain.GetFormat();
    image_info.extent = {extent.width, extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if (vkCreateImage(device, &image_info, nullptr, &m_scene_image) != VK_SUCCESS) {
        LOG_ERROR("Failed to create scene image for FXAA");
        return false;
    }
    
    // Allocate memory for the image
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, m_scene_image, &mem_reqs);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = m_context.FindMemoryType(mem_reqs.memoryTypeBits, 
                                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(device, &alloc_info, nullptr, &m_scene_image_memory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate scene image memory");
        return false;
    }
    
    vkBindImageMemory(device, m_scene_image, m_scene_image_memory, 0);
    
    // Create image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_scene_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = m_swapchain.GetFormat();
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device, &view_info, nullptr, &m_scene_image_view) != VK_SUCCESS) {
        LOG_ERROR("Failed to create scene image view");
        return false;
    }
    
    // Create sampler for scene texture
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    
    if (vkCreateSampler(device, &sampler_info, nullptr, &m_scene_sampler) != VK_SUCCESS) {
        LOG_ERROR("Failed to create scene sampler");
        return false;
    }
    
    // Update FXAA descriptor sets with the new image/sampler
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorImageInfo image_info_desc{};
        image_info_desc.sampler = m_scene_sampler;
        image_info_desc.imageView = m_scene_image_view;
        image_info_desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_fxaa_descriptor_sets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &image_info_desc;
        
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
    
    LOG_INFO("Created FXAA resources");
    return true;
}

void Renderer::DestroyFXAAResources() {
    VkDevice device = m_context.GetDevice();
    
    if (m_scene_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_scene_sampler, nullptr);
        m_scene_sampler = VK_NULL_HANDLE;
    }
    if (m_scene_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_scene_image_view, nullptr);
        m_scene_image_view = VK_NULL_HANDLE;
    }
    if (m_scene_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_scene_image, nullptr);
        m_scene_image = VK_NULL_HANDLE;
    }
    if (m_scene_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_scene_image_memory, nullptr);
        m_scene_image_memory = VK_NULL_HANDLE;
    }
}

bool Renderer::CreateTestMesh() {
    VkDevice device = m_context.GetDevice();
    
    // Simple cube vertices (position, normal, uv)
    struct Vertex {
        float pos[3];
        float normal[3];
        float uv[2];
    };
    
    float h = 1.0f;  // Half size
    std::vector<Vertex> vertices = {
        // Front face (+Z)
        {{-h, -h,  h}, {0, 0, 1}, {0, 0}},
        {{ h, -h,  h}, {0, 0, 1}, {1, 0}},
        {{ h,  h,  h}, {0, 0, 1}, {1, 1}},
        {{-h,  h,  h}, {0, 0, 1}, {0, 1}},
        
        // Back face (-Z)
        {{ h, -h, -h}, {0, 0, -1}, {0, 0}},
        {{-h, -h, -h}, {0, 0, -1}, {1, 0}},
        {{-h,  h, -h}, {0, 0, -1}, {1, 1}},
        {{ h,  h, -h}, {0, 0, -1}, {0, 1}},
        
        // Right face (+X)
        {{ h, -h,  h}, {1, 0, 0}, {0, 0}},
        {{ h, -h, -h}, {1, 0, 0}, {1, 0}},
        {{ h,  h, -h}, {1, 0, 0}, {1, 1}},
        {{ h,  h,  h}, {1, 0, 0}, {0, 1}},
        
        // Left face (-X)
        {{-h, -h, -h}, {-1, 0, 0}, {0, 0}},
        {{-h, -h,  h}, {-1, 0, 0}, {1, 0}},
        {{-h,  h,  h}, {-1, 0, 0}, {1, 1}},
        {{-h,  h, -h}, {-1, 0, 0}, {0, 1}},
        
        // Top face (+Y)
        {{-h,  h,  h}, {0, 1, 0}, {0, 0}},
        {{ h,  h,  h}, {0, 1, 0}, {1, 0}},
        {{ h,  h, -h}, {0, 1, 0}, {1, 1}},
        {{-h,  h, -h}, {0, 1, 0}, {0, 1}},
        
        // Bottom face (-Y)
        {{-h, -h, -h}, {0, -1, 0}, {0, 0}},
        {{ h, -h, -h}, {0, -1, 0}, {1, 0}},
        {{ h, -h,  h}, {0, -1, 0}, {1, 1}},
        {{-h, -h,  h}, {0, -1, 0}, {0, 1}},
    };
    
    std::vector<u32> indices;
    for (u32 face = 0; face < 6; ++face) {
        u32 base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }
    m_test_index_count = static_cast<u32>(indices.size());
    
    VkDeviceSize vertex_size = sizeof(Vertex) * vertices.size();
    VkDeviceSize index_size = sizeof(u32) * indices.size();
    
    // Create vertex buffer
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &buffer_info, nullptr, &m_test_vertex_buffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create test vertex buffer");
        return false;
    }
    
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, m_test_vertex_buffer, &mem_req);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = m_context.FindMemoryType(mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(device, &alloc_info, nullptr, &m_test_vertex_memory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate test vertex memory");
        return false;
    }
    
    vkBindBufferMemory(device, m_test_vertex_buffer, m_test_vertex_memory, 0);
    
    // Copy vertex data
    void* data;
    vkMapMemory(device, m_test_vertex_memory, 0, vertex_size, 0, &data);
    memcpy(data, vertices.data(), vertex_size);
    vkUnmapMemory(device, m_test_vertex_memory);
    
    // Create index buffer
    buffer_info.size = index_size;
    buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    
    if (vkCreateBuffer(device, &buffer_info, nullptr, &m_test_index_buffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create test index buffer");
        return false;
    }
    
    vkGetBufferMemoryRequirements(device, m_test_index_buffer, &mem_req);
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = m_context.FindMemoryType(mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(device, &alloc_info, nullptr, &m_test_index_memory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate test index memory");
        return false;
    }
    
    vkBindBufferMemory(device, m_test_index_buffer, m_test_index_memory, 0);
    
    vkMapMemory(device, m_test_index_memory, 0, index_size, 0, &data);
    memcpy(data, indices.data(), index_size);
    vkUnmapMemory(device, m_test_index_memory);
    
    LOG_INFO("Created test mesh: {} vertices, {} indices", vertices.size(), indices.size());
    return true;
}

void Renderer::RecordCommandBuffer(u32 image_index, const RenderList& render_list) {
    VkCommandBuffer cmd = m_command_buffers[m_current_frame];
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    vkBeginCommandBuffer(cmd, &begin_info);
    
    // Begin render pass - render to offscreen scene texture
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_forward_pass;
    render_pass_info.framebuffer = m_scene_framebuffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = m_swapchain.GetExtent();
    
    // Use a gradient background based on lighting direction
    // This provides visual feedback that rendering is working
    float sun_influence = std::max(0.0f, -m_lighting.sun.direction.y);
    vec3 sky_color = m_lighting.ambient_color * 2.0f + m_lighting.sun.color * sun_influence * 0.3f;
    
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {{sky_color.x, sky_color.y, sky_color.z, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};
    render_pass_info.clearValueCount = static_cast<u32>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();
    
    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain.GetExtent().width);
    viewport.height = static_cast<float>(m_swapchain.GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain.GetExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    
    // Update uniform buffers with current camera and lighting
    UpdateUniformBuffers();
    
    // Bind global descriptor set (camera + lighting) - shared by all pipelines
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout,
                            0, 1, &m_global_descriptor_sets[m_current_frame], 0, nullptr);
    
    // ========================================
    // Draw skybox first (no depth write, fills background)
    // ========================================
    if (m_skybox_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skybox_pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);  // Fullscreen triangle (3 vertices, generated in shader)
    }
    
    // ========================================
    // Draw scene objects with forward pipeline
    // ========================================
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forward_pipeline);
    
    // Draw objects from render list
    if (m_assets) {
        u32 draw_count = 0;
        
        for (const auto& obj : render_list.opaque) {
            MeshData* mesh = m_assets->GetMesh(obj.mesh);
            if (!mesh || !mesh->uploaded) continue;
            if (!mesh->gpu_vertex_buffer) continue;
            
            // Cast void* handles back to VkBuffer
            VkBuffer vertex_buffer = reinterpret_cast<VkBuffer>(mesh->gpu_vertex_buffer);
            VkBuffer index_buffer = reinterpret_cast<VkBuffer>(mesh->gpu_index_buffer);
            
            // Bind mesh buffers
            VkBuffer vertex_buffers[] = {vertex_buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);
            
            // Set push constants for this object
            PushConstants push{};
            push.model = obj.transform;
            push.normalMatrix = obj.transform;  // TODO: Proper normal matrix for non-uniform scale
            
            // Use object's color
            push.color = obj.color;
            
            vkCmdPushConstants(cmd, m_pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &push);
            
            vkCmdDrawIndexed(cmd, mesh->index_count, 1, 0, 0, 0);
            draw_count++;
        }
    } else {
        // Fallback: Draw test mesh if no asset manager
        if (m_test_vertex_buffer != VK_NULL_HANDLE) {
            VkBuffer vertex_buffers[] = {m_test_vertex_buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(cmd, m_test_index_buffer, 0, VK_INDEX_TYPE_UINT32);
            
            PushConstants push{};
            push.model = mat4::identity();
            push.normalMatrix = push.model;
            push.color = vec4{0.8f, 0.6f, 0.4f, 1.0f};
            
            vkCmdPushConstants(cmd, m_pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &push);
            
            vkCmdDrawIndexed(cmd, m_test_index_count, 1, 0, 0, 0);
        }
    }
    
    // ========================================
    // Draw infinite grid (after scene, before UI)
    // ========================================
    if (m_show_grid && m_grid_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_grid_pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);  // Fullscreen triangle
    }
    
    // Call UI render callback (for ImGui rendering) - only if FXAA disabled
    // Otherwise UI renders after FXAA
    if (!m_fxaa_enabled && m_ui_render_callback) {
        m_ui_render_callback(cmd);
    }
    
    vkCmdEndRenderPass(cmd);
    
    // ========================================
    // FXAA Post-Processing Pass
    // ========================================
    if (m_fxaa_enabled && m_fxaa_pipeline != VK_NULL_HANDLE) {
        VkRenderPassBeginInfo fxaa_pass_info{};
        fxaa_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        fxaa_pass_info.renderPass = m_fxaa_pass;
        fxaa_pass_info.framebuffer = m_fxaa_framebuffers[image_index];
        fxaa_pass_info.renderArea.offset = {0, 0};
        fxaa_pass_info.renderArea.extent = m_swapchain.GetExtent();
        
        vkCmdBeginRenderPass(cmd, &fxaa_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        
        // Set viewport and scissor for FXAA pass
        VkViewport fxaa_viewport{};
        fxaa_viewport.x = 0.0f;
        fxaa_viewport.y = 0.0f;
        fxaa_viewport.width = static_cast<float>(m_swapchain.GetExtent().width);
        fxaa_viewport.height = static_cast<float>(m_swapchain.GetExtent().height);
        fxaa_viewport.minDepth = 0.0f;
        fxaa_viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &fxaa_viewport);
        
        VkRect2D fxaa_scissor{};
        fxaa_scissor.offset = {0, 0};
        fxaa_scissor.extent = m_swapchain.GetExtent();
        vkCmdSetScissor(cmd, 0, 1, &fxaa_scissor);
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fxaa_pipeline);
        
        // Bind scene texture descriptor set
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fxaa_pipeline_layout,
                                0, 1, &m_fxaa_descriptor_sets[m_current_frame], 0, nullptr);
        
        // Push FXAA parameters
        struct FXAAParams {
            float texelSizeX;
            float texelSizeY;
            float qualitySubpix;
            float qualityEdgeThreshold;
        } fxaa_params;
        
        fxaa_params.texelSizeX = 1.0f / static_cast<float>(m_swapchain.GetExtent().width);
        fxaa_params.texelSizeY = 1.0f / static_cast<float>(m_swapchain.GetExtent().height);
        fxaa_params.qualitySubpix = 0.75f;
        fxaa_params.qualityEdgeThreshold = 0.166f;
        
        vkCmdPushConstants(cmd, m_fxaa_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(FXAAParams), &fxaa_params);
        
        vkCmdDraw(cmd, 3, 1, 0, 0);  // Fullscreen triangle
        
        // UI renders after FXAA (not anti-aliased, keeps text crisp)
        if (m_ui_render_callback) {
            m_ui_render_callback(cmd);
        }
        
        vkCmdEndRenderPass(cmd);
    }
    
    vkEndCommandBuffer(cmd);
}

void Renderer::DepthPrePass(VkCommandBuffer cmd, const RenderList& render_list) {
    (void)cmd;
    (void)render_list;
    // TODO: Implement depth pre-pass
}

void Renderer::LightClusteringPass(VkCommandBuffer cmd) {
    (void)cmd;
    // TODO: Implement light clustering compute pass
}

void Renderer::ForwardOpaquePass(VkCommandBuffer cmd, const RenderList& render_list) {
    (void)cmd;
    (void)render_list;
    // TODO: Implement forward opaque pass
}

void Renderer::TransparentPass(VkCommandBuffer cmd, const RenderList& render_list) {
    (void)cmd;
    (void)render_list;
    // TODO: Implement transparent pass
}

void Renderer::PostProcessPass(VkCommandBuffer cmd) {
    (void)cmd;
    // TODO: Implement post-processing
}

} // namespace action
