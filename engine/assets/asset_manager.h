#pragma once

#include "core/types.h"
#include "core/jobs/job_system.h"
#include <unordered_map>
#include <queue>
#include <mutex>

namespace action {

// Forward declarations
class VulkanContext;

/*
 * Asset Manager - Streaming Asset System
 * 
 * Optimized for GTX 660 (2GB VRAM):
 * - Texture streaming pool: 800MB
 * - Mesh pool: 300MB  
 * - Upload budget: 2MB/frame
 * - LRU cache eviction
 * - Predictive loading
 */

struct AssetManagerConfig {
    size_t texture_pool_size = 800_MB;
    size_t mesh_pool_size = 300_MB;
    size_t upload_budget_per_frame = 2_MB;
    float prediction_time = 2.0f;
};

// Asset types
enum class AssetType : u8 {
    Mesh,
    Texture,
    Material,
    Shader,
    Audio,
    Animation
};

// Asset load state
enum class AssetState : u8 {
    Unloaded,
    Queued,
    Loading,
    Loaded,
    Failed
};

// Asset metadata
struct AssetInfo {
    std::string path;
    AssetType type;
    size_t size_bytes;
    u32 ref_count = 0;
    float last_access_time = 0;
    float priority = 0;
};

// Mesh asset
struct MeshData {
    std::string name;  // Mesh name for debugging
    std::vector<u8> vertex_data;
    std::vector<u8> index_data;
    u32 vertex_count = 0;
    u32 index_count = 0;
    u32 triangle_count = 0;
    AABB bounds;
    
    // Higher-level vertex/index access (for imports)
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    
    // Convert vertices to raw data
    void PackVertexData() {
        if (!vertices.empty()) {
            vertex_count = (u32)vertices.size();
            vertex_data.resize(vertex_count * sizeof(float) * 8);  // pos(3) + normal(3) + uv(2)
            float* dst = reinterpret_cast<float*>(vertex_data.data());
            for (const auto& v : vertices) {
                *dst++ = v.position.x;
                *dst++ = v.position.y;
                *dst++ = v.position.z;
                *dst++ = v.normal.x;
                *dst++ = v.normal.y;
                *dst++ = v.normal.z;
                *dst++ = v.uv.x;
                *dst++ = v.uv.y;
            }
        }
        if (!indices.empty()) {
            index_count = (u32)indices.size();
            triangle_count = index_count / 3;
            index_data.resize(index_count * sizeof(u32));
            memcpy(index_data.data(), indices.data(), index_data.size());
        }
    }
    
    // GPU resources (filled after upload) - stored as void* for header decoupling
    // These are VkBuffer and VkDeviceMemory handles, cast in vulkan code
    void* gpu_vertex_buffer = nullptr;
    void* gpu_vertex_memory = nullptr;
    void* gpu_index_buffer = nullptr;
    void* gpu_index_memory = nullptr;
    bool uploaded = false;
};

// Texture asset
struct TextureData {
    std::vector<u8> pixel_data;
    u32 width = 0;
    u32 height = 0;
    u32 mip_levels = 1;
    u32 format = 0;  // VkFormat
    
    // GPU resource
    void* gpu_image = nullptr;
};

// Material asset
struct MaterialData {
    ShaderHandle shader;
    TextureHandle diffuse;
    TextureHandle normal;
    TextureHandle mask;  // R=metallic, G=roughness, B=AO
    vec4 base_color = {1, 1, 1, 1};
    float metallic = 0;
    float roughness = 0.5f;
};

// Load request
struct LoadRequest {
    std::string path;
    AssetType type;
    float priority;
    std::function<void(bool success)> callback;
    
    bool operator<(const LoadRequest& o) const {
        return priority < o.priority;  // Higher priority first
    }
};

class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager() = default;
    
    bool Initialize(const AssetManagerConfig& config);
    void Shutdown();
    
    // Set Vulkan context for GPU uploads
    void SetVulkanContext(VulkanContext* context) { m_vulkan_context = context; }
    
    // Per-frame update (process load queue, upload to GPU)
    void Update(size_t upload_budget);
    
    // Asset loading (async)
    MeshHandle LoadMesh(const std::string& path, float priority = 0);
    TextureHandle LoadTexture(const std::string& path, float priority = 0);
    MaterialHandle LoadMaterial(const std::string& path, float priority = 0);
    
    // Synchronous loading (blocks)
    MeshHandle LoadMeshSync(const std::string& path);
    TextureHandle LoadTextureSync(const std::string& path);
    
    // Procedural mesh creation (for test scenes)
    MeshHandle CreatePlaneMesh(float width, float depth, u32 segments_x = 1, u32 segments_z = 1);
    MeshHandle CreateCubeMesh(float size = 1.0f);
    MeshHandle CreateSphereMesh(float radius = 1.0f, u32 segments = 16);
    
    // Create mesh from imported data
    MeshHandle CreateMesh(MeshData& mesh_data);
    
    // Get loaded assets
    MeshData* GetMesh(MeshHandle handle);
    TextureData* GetTexture(TextureHandle handle);
    MaterialData* GetMaterial(MaterialHandle handle);
    
    // Asset state
    AssetState GetMeshState(MeshHandle handle) const;
    AssetState GetTextureState(TextureHandle handle) const;
    
    // Reference counting
    void AddRef(MeshHandle handle);
    void Release(MeshHandle handle);
    void AddRef(TextureHandle handle);
    void Release(TextureHandle handle);
    
    // Preloading (for known assets)
    void PreloadAssets(const std::vector<std::string>& paths);
    
    // Stats
    size_t GetBytesUploadedThisFrame() const { return m_bytes_uploaded; }
    size_t GetTexturePoolUsage() const { return m_texture_pool_used; }
    size_t GetMeshPoolUsage() const { return m_mesh_pool_used; }
    u32 GetPendingLoadCount() const;
    
private:
    // Internal loading
    bool LoadMeshFromFile(const std::string& path, MeshData& out_data);
    bool LoadTextureFromFile(const std::string& path, TextureData& out_data);
    
    // GPU upload
    bool UploadMesh(MeshHandle handle);
    bool UploadTexture(TextureHandle handle);
    
    // Cache management (LRU eviction)
    void EvictLRU(AssetType type, size_t bytes_needed);
    
    // Handle allocation
    template<typename T>
    Handle<T> AllocateHandle();
    
    AssetManagerConfig m_config;
    
    // Asset storage
    std::unordered_map<u32, MeshData> m_meshes;
    std::unordered_map<u32, TextureData> m_textures;
    std::unordered_map<u32, MaterialData> m_materials;
    
    // Asset info (path -> handle mapping)
    std::unordered_map<std::string, MeshHandle> m_mesh_paths;
    std::unordered_map<std::string, TextureHandle> m_texture_paths;
    
    // Asset state
    std::unordered_map<u32, AssetState> m_mesh_states;
    std::unordered_map<u32, AssetState> m_texture_states;
    
    // Load queue
    std::mutex m_queue_mutex;
    std::priority_queue<LoadRequest> m_load_queue;
    
    // Handle counters
    u32 m_next_mesh_handle = 1;
    u32 m_next_texture_handle = 1;
    u32 m_next_material_handle = 1;
    
    // Memory tracking
    size_t m_texture_pool_used = 0;
    size_t m_mesh_pool_used = 0;
    size_t m_bytes_uploaded = 0;
    
    // Job system reference
    JobSystem* m_jobs = nullptr;
    
    // Vulkan context for GPU uploads
    VulkanContext* m_vulkan_context = nullptr;
};

} // namespace action
