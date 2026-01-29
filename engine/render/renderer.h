#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "platform/vulkan/vulkan_context.h"
#include "platform/vulkan/vulkan_swapchain.h"
#include <vector>
#include <functional>

namespace action {

/*
 * Forward+ Renderer
 * 
 * Optimized for GTX 660 (2GB VRAM):
 * - Forward+ with light clustering
 * - Simple depth pre-pass
 * - Conservative draw budgets
 * - Triple buffering
 * 
 * Frame budget: 16.67ms
 * Draw call budget: 2500
 * Triangle budget: 800K
 */

struct RendererConfig {
    void* window;
    u32 width;
    u32 height;
    bool vsync;
    size_t vram_budget;
    u32 max_draw_calls;
    u32 shadow_cascade_count;
    u32 shadow_resolution;
};

// Renderable object data
struct RenderObject {
    MeshHandle mesh;
    MaterialHandle material;
    mat4 transform;
    AABB bounds;
    vec4 color{0.8f, 0.8f, 0.8f, 1.0f};  // Object color (default light gray)
    float distance_sq;  // From camera, for sorting
    u8 lod_level;
};

// Render list (populated by world manager)
struct RenderList {
    std::vector<RenderObject> opaque;
    std::vector<RenderObject> transparent;
    u32 total_triangles = 0;
    u32 total_draw_calls = 0;
    
    void Clear() {
        opaque.clear();
        transparent.clear();
        total_triangles = 0;
        total_draw_calls = 0;
    }
};

// Camera for rendering
struct Camera {
    vec3 position;
    vec3 forward;
    vec3 up;
    
    float fov = Radians(75.0f);     // Wide FOV for action games
    float near_plane = 0.1f;
    float far_plane = 2000.0f;      // Long draw distance
    float aspect = 16.0f / 9.0f;
    
    mat4 GetViewMatrix() const;
    mat4 GetProjectionMatrix() const;
    mat4 GetViewProjectionMatrix() const;
    Frustum GetFrustum() const;
};

// Light types
struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
    bool cast_shadows;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
    bool cast_shadows;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float radius;
    float inner_angle;
    float outer_angle;
    bool cast_shadows;
};

// Lighting data
struct LightingData {
    DirectionalLight sun;
    std::vector<PointLight> point_lights;
    std::vector<SpotLight> spot_lights;
    vec3 ambient_color = {0.1f, 0.12f, 0.15f};
};

// Forward declaration
class AssetManager;

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;
    
    bool Initialize(const RendererConfig& config);
    void Shutdown();
    
    // Set asset manager reference for mesh data access
    void SetAssetManager(AssetManager* assets) { m_assets = assets; }
    
    // Frame rendering
    void BeginFrame();
    void RenderScene(const RenderList& render_list);
    void EndFrame();
    
    // Camera
    void SetCamera(const Camera& camera);
    Camera& GetCamera() { return m_camera; }
    const Camera& GetCamera() const { return m_camera; }
    
    // Lighting
    void SetLighting(const LightingData& lighting);
    
    // Stats
    u32 GetDrawCallCount() const { return m_stats.draw_calls; }
    u32 GetTriangleCount() const { return m_stats.triangles; }
    size_t GetVRAMUsage() const { return m_stats.vram_used; }
    
    // Editor grid
    void SetGridVisible(bool visible) { m_show_grid = visible; }
    bool IsGridVisible() const { return m_show_grid; }
    
    // Resize handling
    void OnResize(u32 width, u32 height);
    
    // Access to Vulkan context (for asset loading)
    VulkanContext& GetContext() { return m_context; }
    
    // Access to render pass (for ImGui)
    VkRenderPass GetForwardPass() const { return m_forward_pass; }
    
    // Get current command buffer (for editor UI rendering)
    VkCommandBuffer GetCurrentCommandBuffer() const { return m_command_buffers[m_current_frame]; }
    
    // Set a callback to be called during render pass (for UI/overlay rendering)
    using RenderCallback = std::function<void(VkCommandBuffer)>;
    void SetUIRenderCallback(RenderCallback callback) { m_ui_render_callback = callback; }
    
private:
    bool CreateRenderPasses();
    bool CreateFramebuffers();
    bool CreatePipelines();
    bool CreateSyncObjects();
    bool CreateCommandBuffers();
    bool CreateDescriptorSets();
    bool CreateUniformBuffers();
    
    // Shader loading
    VkShaderModule LoadShaderModule(const std::string& path);
    
    // Buffer helpers
    void UpdateUniformBuffers();
    
    void RecordCommandBuffer(u32 image_index, const RenderList& render_list);
    void DepthPrePass(VkCommandBuffer cmd, const RenderList& render_list);
    void LightClusteringPass(VkCommandBuffer cmd);
    void ForwardOpaquePass(VkCommandBuffer cmd, const RenderList& render_list);
    void TransparentPass(VkCommandBuffer cmd, const RenderList& render_list);
    void PostProcessPass(VkCommandBuffer cmd);
    
    RendererConfig m_config;
    
    VulkanContext m_context;
    VulkanSwapchain m_swapchain;
    
    Camera m_camera;
    LightingData m_lighting;
    
    // Asset manager reference
    AssetManager* m_assets = nullptr;
    
    // Render passes
    VkRenderPass m_depth_pass = VK_NULL_HANDLE;
    VkRenderPass m_forward_pass = VK_NULL_HANDLE;
    
    // Pipelines
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_forward_pipeline = VK_NULL_HANDLE;
    VkPipeline m_skybox_pipeline = VK_NULL_HANDLE;
    VkPipeline m_grid_pipeline = VK_NULL_HANDLE;
    bool m_show_grid = true;  // Toggle for editor grid
    
    // Synchronization (triple buffering)
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 3;
    
    // Descriptors
    VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_global_descriptor_sets;
    
    // Uniform buffers (per frame in flight)
    struct UniformBuffers {
        VkBuffer camera_buffer = VK_NULL_HANDLE;
        VkDeviceMemory camera_memory = VK_NULL_HANDLE;
        void* camera_mapped = nullptr;
        
        VkBuffer lighting_buffer = VK_NULL_HANDLE;
        VkDeviceMemory lighting_memory = VK_NULL_HANDLE;
        void* lighting_mapped = nullptr;
    };
    std::array<UniformBuffers, MAX_FRAMES_IN_FLIGHT> m_uniform_buffers;
    
    // Test mesh (for debugging rendering pipeline)
    VkBuffer m_test_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_test_vertex_memory = VK_NULL_HANDLE;
    VkBuffer m_test_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_test_index_memory = VK_NULL_HANDLE;
    u32 m_test_index_count = 0;
    bool CreateTestMesh();
    
    // Framebuffers
    std::vector<VkFramebuffer> m_framebuffers;
    
    // Command buffers
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_command_buffers;
    
    // Synchronization
    // Per-frame-in-flight resources (for command buffer/uniform buffer double/triple buffering)
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_image_available;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_render_finished;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences;
    
    // Per-swapchain-image semaphores for proper present synchronization
    // This prevents semaphore reuse issues with non-sequential swapchain images
    std::vector<VkSemaphore> m_image_render_finished;  // One per swapchain image
    
    // Track which fence is associated with each swapchain image
    // This prevents using an image that's still being rendered
    std::vector<VkFence> m_images_in_flight;
    u32 m_current_frame = 0;
    
    // UI render callback (for editor ImGui rendering)
    RenderCallback m_ui_render_callback;
    
    // Stats
    struct RenderStats {
        u32 draw_calls = 0;
        u32 triangles = 0;
        size_t vram_used = 0;
    } m_stats;
};

} // namespace action