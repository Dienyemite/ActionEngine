#include "asset_manager.h"
#include "core/logging.h"
#include "core/profiler.h"
#include "core/math/math.h"
#include "platform/vulkan/vulkan_context.h"
#include <fstream>
#include <cstring>

namespace action {

bool AssetManager::Initialize(const AssetManagerConfig& config) {
    m_config = config;
    
    LOG_INFO("AssetManager initialized");
    LOG_INFO("  Texture pool: {} MB", config.texture_pool_size / (1024 * 1024));
    LOG_INFO("  Mesh pool: {} MB", config.mesh_pool_size / (1024 * 1024));
    LOG_INFO("  Upload budget: {} MB/frame", config.upload_budget_per_frame / (1024 * 1024));
    
    return true;
}

void AssetManager::Shutdown() {
    // Release all GPU resources
    if (m_vulkan_context) {
        VkDevice device = m_vulkan_context->GetDevice();
        
        // Wait for GPU to finish any pending work
        vkDeviceWaitIdle(device);
        
        // Destroy mesh GPU resources
        for (auto& [handle, mesh] : m_meshes) {
            if (mesh.gpu_vertex_buffer) {
                vkDestroyBuffer(device, reinterpret_cast<VkBuffer>(mesh.gpu_vertex_buffer), nullptr);
                mesh.gpu_vertex_buffer = nullptr;
            }
            if (mesh.gpu_vertex_memory) {
                vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(mesh.gpu_vertex_memory), nullptr);
                mesh.gpu_vertex_memory = nullptr;
            }
            if (mesh.gpu_index_buffer) {
                vkDestroyBuffer(device, reinterpret_cast<VkBuffer>(mesh.gpu_index_buffer), nullptr);
                mesh.gpu_index_buffer = nullptr;
            }
            if (mesh.gpu_index_memory) {
                vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(mesh.gpu_index_memory), nullptr);
                mesh.gpu_index_memory = nullptr;
            }
        }
    }
    
    m_meshes.clear();
    m_textures.clear();
    m_materials.clear();
    m_mesh_paths.clear();
    m_texture_paths.clear();
    
    LOG_INFO("AssetManager shutdown");
}

void AssetManager::Update(size_t upload_budget) {
    PROFILE_SCOPE("AssetManager::Update");
    
    m_bytes_uploaded = 0;
    
    // Process load queue
    while (m_bytes_uploaded < upload_budget) {
        LoadRequest request;
        
        {
            std::lock_guard lock(m_queue_mutex);
            if (m_load_queue.empty()) break;
            
            request = m_load_queue.top();
            m_load_queue.pop();
        }
        
        bool success = false;
        
        switch (request.type) {
            case AssetType::Mesh: {
                auto it = m_mesh_paths.find(request.path);
                if (it != m_mesh_paths.end()) {
                    success = UploadMesh(it->second);
                }
                break;
            }
            case AssetType::Texture: {
                auto it = m_texture_paths.find(request.path);
                if (it != m_texture_paths.end()) {
                    success = UploadTexture(it->second);
                }
                break;
            }
            default:
                break;
        }
        
        if (request.callback) {
            request.callback(success);
        }
    }
}

MeshHandle AssetManager::LoadMesh(const std::string& path, float priority) {
    // Check if already loaded
    auto it = m_mesh_paths.find(path);
    if (it != m_mesh_paths.end()) {
        AddRef(it->second);
        return it->second;
    }
    
    // Allocate handle
    MeshHandle handle;
    handle.index = m_next_mesh_handle++;
    handle.generation = 1;
    
    m_mesh_paths[path] = handle;
    m_mesh_states[handle.index] = AssetState::Queued;
    
    // Queue for loading
    LoadRequest request;
    request.path = path;
    request.type = AssetType::Mesh;
    request.priority = priority;
    
    {
        std::lock_guard lock(m_queue_mutex);
        m_load_queue.push(request);
    }
    
    return handle;
}

TextureHandle AssetManager::LoadTexture(const std::string& path, float priority) {
    // Check if already loaded
    auto it = m_texture_paths.find(path);
    if (it != m_texture_paths.end()) {
        AddRef(it->second);
        return it->second;
    }
    
    // Allocate handle
    TextureHandle handle;
    handle.index = m_next_texture_handle++;
    handle.generation = 1;
    
    m_texture_paths[path] = handle;
    m_texture_states[handle.index] = AssetState::Queued;
    
    // Queue for loading
    LoadRequest request;
    request.path = path;
    request.type = AssetType::Texture;
    request.priority = priority;
    
    {
        std::lock_guard lock(m_queue_mutex);
        m_load_queue.push(request);
    }
    
    return handle;
}

MaterialHandle AssetManager::LoadMaterial(const std::string& path, float priority) {
    (void)priority;
    
    MaterialHandle handle;
    handle.index = m_next_material_handle++;
    handle.generation = 1;
    
    // TODO: Load material data from file
    m_materials[handle.index] = MaterialData{};
    
    return handle;
}

MeshHandle AssetManager::LoadMeshSync(const std::string& path) {
    MeshHandle handle = LoadMesh(path, 1000.0f);  // High priority
    
    // Load immediately
    MeshData data;
    if (LoadMeshFromFile(path, data)) {
        m_meshes[handle.index] = std::move(data);
        m_mesh_states[handle.index] = AssetState::Loaded;
        UploadMesh(handle);
    } else {
        m_mesh_states[handle.index] = AssetState::Failed;
    }
    
    return handle;
}

TextureHandle AssetManager::LoadTextureSync(const std::string& path) {
    TextureHandle handle = LoadTexture(path, 1000.0f);
    
    TextureData data;
    if (LoadTextureFromFile(path, data)) {
        m_textures[handle.index] = std::move(data);
        m_texture_states[handle.index] = AssetState::Loaded;
        UploadTexture(handle);
    } else {
        m_texture_states[handle.index] = AssetState::Failed;
    }
    
    return handle;
}

MeshData* AssetManager::GetMesh(MeshHandle handle) {
    auto it = m_meshes.find(handle.index);
    if (it != m_meshes.end()) {
        return &it->second;
    }
    return nullptr;
}

TextureData* AssetManager::GetTexture(TextureHandle handle) {
    auto it = m_textures.find(handle.index);
    if (it != m_textures.end()) {
        return &it->second;
    }
    return nullptr;
}

MaterialData* AssetManager::GetMaterial(MaterialHandle handle) {
    auto it = m_materials.find(handle.index);
    if (it != m_materials.end()) {
        return &it->second;
    }
    return nullptr;
}

AssetState AssetManager::GetMeshState(MeshHandle handle) const {
    auto it = m_mesh_states.find(handle.index);
    if (it != m_mesh_states.end()) {
        return it->second;
    }
    return AssetState::Unloaded;
}

AssetState AssetManager::GetTextureState(TextureHandle handle) const {
    auto it = m_texture_states.find(handle.index);
    if (it != m_texture_states.end()) {
        return it->second;
    }
    return AssetState::Unloaded;
}

void AssetManager::AddRef(MeshHandle handle) {
    // TODO: Implement reference counting
    (void)handle;
}

void AssetManager::Release(MeshHandle handle) {
    // TODO: Implement reference counting
    (void)handle;
}

void AssetManager::AddRef(TextureHandle handle) {
    (void)handle;
}

void AssetManager::Release(TextureHandle handle) {
    (void)handle;
}

void AssetManager::PreloadAssets(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        // Determine type from extension
        if (path.ends_with(".mesh") || path.ends_with(".obj")) {
            LoadMesh(path, 10.0f);
        } else if (path.ends_with(".tex") || path.ends_with(".png") || path.ends_with(".jpg")) {
            LoadTexture(path, 10.0f);
        }
    }
}

u32 AssetManager::GetPendingLoadCount() const {
    std::lock_guard lock(const_cast<std::mutex&>(m_queue_mutex));
    return static_cast<u32>(m_load_queue.size());
}

bool AssetManager::LoadMeshFromFile(const std::string& path, MeshData& out_data) {
    // TODO: Implement actual mesh loading (OBJ, custom format, etc.)
    LOG_DEBUG("Loading mesh: {}", path);
    
    // Placeholder - create simple quad
    out_data.vertex_count = 4;
    out_data.index_count = 6;
    out_data.triangle_count = 2;
    
    return true;
}

bool AssetManager::LoadTextureFromFile(const std::string& path, TextureData& out_data) {
    // TODO: Implement actual texture loading (PNG, custom format with BC compression)
    LOG_DEBUG("Loading texture: {}", path);
    
    // Placeholder
    out_data.width = 1;
    out_data.height = 1;
    out_data.mip_levels = 1;
    out_data.pixel_data = {255, 255, 255, 255};
    
    return true;
}

bool AssetManager::UploadMesh(MeshHandle handle) {
    auto* mesh = GetMesh(handle);
    if (!mesh) return false;
    if (mesh->uploaded) return true;  // Already uploaded
    if (!m_vulkan_context) {
        LOG_ERROR("Cannot upload mesh: VulkanContext not set");
        return false;
    }
    
    VkDevice device = m_vulkan_context->GetDevice();
    
    // Create vertex buffer
    VkDeviceSize vertex_size = mesh->vertex_data.size();
    if (vertex_size > 0) {
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
        
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = vertex_size;
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device, &buffer_info, nullptr, &vertex_buffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to create vertex buffer for mesh");
            return false;
        }
        
        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, vertex_buffer, &mem_req);
        
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = m_vulkan_context->FindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(device, &alloc_info, nullptr, &vertex_memory) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate vertex buffer memory");
            vkDestroyBuffer(device, vertex_buffer, nullptr);
            return false;
        }
        
        vkBindBufferMemory(device, vertex_buffer, vertex_memory, 0);
        
        // Copy data
        void* data;
        vkMapMemory(device, vertex_memory, 0, vertex_size, 0, &data);
        memcpy(data, mesh->vertex_data.data(), vertex_size);
        vkUnmapMemory(device, vertex_memory);
        
        // Store as void* for header decoupling
        mesh->gpu_vertex_buffer = reinterpret_cast<void*>(vertex_buffer);
        mesh->gpu_vertex_memory = reinterpret_cast<void*>(vertex_memory);
    }
    
    // Create index buffer
    VkDeviceSize index_size = mesh->index_data.size();
    if (index_size > 0) {
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory index_memory = VK_NULL_HANDLE;
        
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = index_size;
        buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device, &buffer_info, nullptr, &index_buffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to create index buffer for mesh");
            return false;
        }
        
        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, index_buffer, &mem_req);
        
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = m_vulkan_context->FindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(device, &alloc_info, nullptr, &index_memory) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate index buffer memory");
            vkDestroyBuffer(device, index_buffer, nullptr);
            return false;
        }
        
        vkBindBufferMemory(device, index_buffer, index_memory, 0);
        
        // Copy data
        void* data;
        vkMapMemory(device, index_memory, 0, index_size, 0, &data);
        memcpy(data, mesh->index_data.data(), index_size);
        vkUnmapMemory(device, index_memory);
        
        // Store as void* for header decoupling
        mesh->gpu_index_buffer = reinterpret_cast<void*>(index_buffer);
        mesh->gpu_index_memory = reinterpret_cast<void*>(index_memory);
    }
    
    mesh->uploaded = true;
    size_t size = vertex_size + index_size;
    m_bytes_uploaded += size;
    m_mesh_pool_used += size;
    m_mesh_states[handle.index] = AssetState::Loaded;
    
    LOG_DEBUG("Uploaded mesh to GPU: {} vertices, {} indices ({} bytes)",
              mesh->vertex_count, mesh->index_count, size);
    
    return true;
}

bool AssetManager::UploadTexture(TextureHandle handle) {
    auto* texture = GetTexture(handle);
    if (!texture) return false;
    
    // TODO: Create Vulkan image and upload
    size_t size = texture->pixel_data.size();
    m_bytes_uploaded += size;
    m_texture_pool_used += size;
    
    m_texture_states[handle.index] = AssetState::Loaded;
    
    return true;
}

void AssetManager::EvictLRU(AssetType type, size_t bytes_needed) {
    // TODO: Implement LRU eviction based on last_access_time
    (void)type;
    (void)bytes_needed;
}

// Tightly-packed vertex format for procedural meshes (matches shader layout)
// Note: We can't use vec3/vec2 here because they have SIMD alignment padding
#pragma pack(push, 1)
struct ProceduralVertex {
    float px, py, pz;    // position (12 bytes)
    float nx, ny, nz;    // normal (12 bytes)  
    float u, v;          // uv (8 bytes)
    // Total: 32 bytes - matches shader stride
};
#pragma pack(pop)

MeshHandle AssetManager::CreatePlaneMesh(float width, float depth, u32 segments_x, u32 segments_z) {
    MeshHandle handle;
    handle.index = m_next_mesh_handle++;
    handle.generation = 1;
    
    MeshData mesh;
    
    u32 vertex_count = (segments_x + 1) * (segments_z + 1);
    u32 index_count = segments_x * segments_z * 6;
    
    std::vector<ProceduralVertex> vertices;
    vertices.reserve(vertex_count);
    
    std::vector<u32> indices;
    indices.reserve(index_count);
    
    float half_width = width * 0.5f;
    float half_depth = depth * 0.5f;
    
    // Generate vertices
    for (u32 z = 0; z <= segments_z; ++z) {
        for (u32 x = 0; x <= segments_x; ++x) {
            float u_coord = static_cast<float>(x) / segments_x;
            float v_coord = static_cast<float>(z) / segments_z;
            
            ProceduralVertex vert;
            vert.px = -half_width + u_coord * width;
            vert.py = 0.0f;
            vert.pz = -half_depth + v_coord * depth;
            vert.nx = 0.0f;
            vert.ny = 1.0f;
            vert.nz = 0.0f;
            vert.u = u_coord;
            vert.v = v_coord;
            
            vertices.push_back(vert);
        }
    }
    
    // Generate indices
    for (u32 z = 0; z < segments_z; ++z) {
        for (u32 x = 0; x < segments_x; ++x) {
            u32 base = z * (segments_x + 1) + x;
            
            indices.push_back(base);
            indices.push_back(base + segments_x + 1);
            indices.push_back(base + 1);
            
            indices.push_back(base + 1);
            indices.push_back(base + segments_x + 1);
            indices.push_back(base + segments_x + 2);
        }
    }
    
    // Copy to mesh data
    mesh.vertex_count = vertex_count;
    mesh.index_count = index_count;
    mesh.triangle_count = index_count / 3;
    
    mesh.vertex_data.resize(vertices.size() * sizeof(ProceduralVertex));
    std::memcpy(mesh.vertex_data.data(), vertices.data(), mesh.vertex_data.size());
    
    mesh.index_data.resize(indices.size() * sizeof(u32));
    std::memcpy(mesh.index_data.data(), indices.data(), mesh.index_data.size());
    
    mesh.bounds = AABB(
        vec3{-half_width, 0, -half_depth},
        vec3{half_width, 0, half_depth}
    );
    
    m_meshes[handle.index] = std::move(mesh);
    m_mesh_states[handle.index] = AssetState::Loaded;
    UploadMesh(handle);
    
    LOG_DEBUG("Created plane mesh: {}x{} segments, {} vertices, {} triangles",
              segments_x, segments_z, vertex_count, index_count / 3);
    
    return handle;
}

MeshHandle AssetManager::CreateCubeMesh(float size) {
    MeshHandle handle;
    handle.index = m_next_mesh_handle++;
    handle.generation = 1;
    
    MeshData mesh;
    
    float h = size * 0.5f;
    
    // 24 vertices (4 per face for proper normals)
    std::vector<ProceduralVertex> vertices = {
        // Front face (+Z)
        {-h, -h,  h,  0, 0, 1,  0, 0},
        { h, -h,  h,  0, 0, 1,  1, 0},
        { h,  h,  h,  0, 0, 1,  1, 1},
        {-h,  h,  h,  0, 0, 1,  0, 1},
        
        // Back face (-Z)
        { h, -h, -h,  0, 0, -1,  0, 0},
        {-h, -h, -h,  0, 0, -1,  1, 0},
        {-h,  h, -h,  0, 0, -1,  1, 1},
        { h,  h, -h,  0, 0, -1,  0, 1},
        
        // Right face (+X)
        { h, -h,  h,  1, 0, 0,  0, 0},
        { h, -h, -h,  1, 0, 0,  1, 0},
        { h,  h, -h,  1, 0, 0,  1, 1},
        { h,  h,  h,  1, 0, 0,  0, 1},
        
        // Left face (-X)
        {-h, -h, -h,  -1, 0, 0,  0, 0},
        {-h, -h,  h,  -1, 0, 0,  1, 0},
        {-h,  h,  h,  -1, 0, 0,  1, 1},
        {-h,  h, -h,  -1, 0, 0,  0, 1},
        
        // Top face (+Y)
        {-h,  h,  h,  0, 1, 0,  0, 0},
        { h,  h,  h,  0, 1, 0,  1, 0},
        { h,  h, -h,  0, 1, 0,  1, 1},
        {-h,  h, -h,  0, 1, 0,  0, 1},
        
        // Bottom face (-Y)
        {-h, -h, -h,  0, -1, 0,  0, 0},
        { h, -h, -h,  0, -1, 0,  1, 0},
        { h, -h,  h,  0, -1, 0,  1, 1},
        {-h, -h,  h,  0, -1, 0,  0, 1},
    };
    
    std::vector<u32> indices;
    for (u32 face = 0; face < 6; ++face) {
        u32 base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    
    mesh.vertex_count = 24;
    mesh.index_count = 36;
    mesh.triangle_count = 12;
    
    mesh.vertex_data.resize(vertices.size() * sizeof(ProceduralVertex));
    std::memcpy(mesh.vertex_data.data(), vertices.data(), mesh.vertex_data.size());
    
    mesh.index_data.resize(indices.size() * sizeof(u32));
    std::memcpy(mesh.index_data.data(), indices.data(), mesh.index_data.size());
    
    mesh.bounds = AABB(vec3{-h, -h, -h}, vec3{h, h, h});
    
    m_meshes[handle.index] = std::move(mesh);
    m_mesh_states[handle.index] = AssetState::Loaded;
    UploadMesh(handle);
    
    LOG_DEBUG("Created cube mesh: size={}, 24 vertices, 12 triangles", size);
    
    return handle;
}

MeshHandle AssetManager::CreateSphereMesh(float radius, u32 segments) {
    MeshHandle handle;
    handle.index = m_next_mesh_handle++;
    handle.generation = 1;
    
    MeshData mesh;
    
    std::vector<ProceduralVertex> vertices;
    std::vector<u32> indices;
    
    u32 rings = segments;
    u32 sectors = segments * 2;
    
    // Generate vertices
    for (u32 r = 0; r <= rings; ++r) {
        float phi = PI * static_cast<float>(r) / rings;
        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);
        
        for (u32 s = 0; s <= sectors; ++s) {
            float theta = TWO_PI * static_cast<float>(s) / sectors;
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);
            
            float pos_x = radius * sin_phi * cos_theta;
            float pos_y = radius * cos_phi;
            float pos_z = radius * sin_phi * sin_theta;
            
            // Normalize for normal
            float len = std::sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z);
            float inv_len = (len > 0.0001f) ? 1.0f / len : 0.0f;
            
            ProceduralVertex vert;
            vert.px = pos_x;
            vert.py = pos_y;
            vert.pz = pos_z;
            vert.nx = pos_x * inv_len;
            vert.ny = pos_y * inv_len;
            vert.nz = pos_z * inv_len;
            vert.u = static_cast<float>(s) / sectors;
            vert.v = static_cast<float>(r) / rings;
            
            vertices.push_back(vert);
        }
    }
    
    // Generate indices
    for (u32 r = 0; r < rings; ++r) {
        for (u32 s = 0; s < sectors; ++s) {
            u32 current = r * (sectors + 1) + s;
            u32 next = current + sectors + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    mesh.vertex_count = static_cast<u32>(vertices.size());
    mesh.index_count = static_cast<u32>(indices.size());
    mesh.triangle_count = mesh.index_count / 3;
    
    mesh.vertex_data.resize(vertices.size() * sizeof(ProceduralVertex));
    std::memcpy(mesh.vertex_data.data(), vertices.data(), mesh.vertex_data.size());
    
    mesh.index_data.resize(indices.size() * sizeof(u32));
    std::memcpy(mesh.index_data.data(), indices.data(), mesh.index_data.size());
    
    mesh.bounds = AABB(vec3{-radius, -radius, -radius}, vec3{radius, radius, radius});
    
    m_meshes[handle.index] = std::move(mesh);
    m_mesh_states[handle.index] = AssetState::Loaded;
    UploadMesh(handle);
    
    LOG_DEBUG("Created sphere mesh: radius={}, {} vertices, {} triangles",
              radius, mesh.vertex_count, mesh.triangle_count);
    
    return handle;
}

MeshHandle AssetManager::CreateMesh(MeshData& mesh_data) {
    MeshHandle handle;
    handle.index = m_next_mesh_handle++;
    handle.generation = 1;
    
    // Pack high-level vertices/indices into raw data if provided
    mesh_data.PackVertexData();
    
    m_meshes[handle.index] = std::move(mesh_data);
    m_mesh_states[handle.index] = AssetState::Loaded;
    UploadMesh(handle);
    
    MeshData* stored = GetMesh(handle);
    LOG_DEBUG("Created mesh '{}': {} vertices, {} triangles",
              stored->name, stored->vertex_count, stored->triangle_count);
    
    return handle;
}

} // namespace action
