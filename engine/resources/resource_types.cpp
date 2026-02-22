#include "resource_types.h"
#include "core/logging.h"
#include <fstream>

namespace action {

// ===== TextureResource =====

bool TextureResource::Load() {
    // Actual texture loading will be implemented with image loader
    // For now, just mark as loaded
    SetState(ResourceState::Loaded);
    return true;
}

void TextureResource::Unload() {
    // Destroy the GPU-side resource before dropping the CPU copy.
    // If the renderer registered a release callback (via SetGPUReleaseCallback),
    // call it now; otherwise the VkImage/VkImageView would be leaked.
    if (m_gpu_handle && m_gpu_release_func) {
        m_gpu_release_func(m_gpu_handle);
    }
    m_gpu_handle = nullptr;
    m_gpu_release_func = nullptr;
    m_pixel_data.clear();
    m_pixel_data.shrink_to_fit();
}

// ===== MeshResource =====

bool MeshResource::Load() {
    // Actual mesh loading will be implemented with OBJ/GLTF loader
    SetState(ResourceState::Loaded);
    return true;
}

void MeshResource::Unload() {
    // Destroy GPU buffers before freeing CPU data.
    if (m_vertex_buffer && m_vb_release_func) { m_vb_release_func(m_vertex_buffer); }
    if (m_index_buffer  && m_ib_release_func) { m_ib_release_func(m_index_buffer);  }
    m_vertex_buffer = nullptr;
    m_index_buffer  = nullptr;
    m_vb_release_func = nullptr;
    m_ib_release_func = nullptr;
    m_vertices.clear();
    m_vertices.shrink_to_fit();
    m_indices.clear();
    m_indices.shrink_to_fit();
}

void MeshResource::CalculateBounds() {
    m_bounds = AABB();
    for (const auto& vertex : m_vertices) {
        m_bounds.expand(vertex.position);
    }
}

// ===== MaterialResource =====

bool MaterialResource::Load() {
    // Material loading from file
    SetState(ResourceState::Loaded);
    return true;
}

void MaterialResource::Unload() {
    m_albedo.reset();
    m_normal.reset();
    m_metallic_roughness.reset();
}

// ===== ShaderResource =====

bool ShaderResource::Load() {
    const std::string& path = GetPath();
    bool loaded_any = false;
    
    // Load vertex shader
    std::string vert_path = path + ".vert.spv";
    std::ifstream vert_file(vert_path, std::ios::binary | std::ios::ate);
    if (vert_file.is_open()) {
        size_t size = vert_file.tellg();
        vert_file.seekg(0);
        m_vertex_code.resize(size / sizeof(u32));
        vert_file.read(reinterpret_cast<char*>(m_vertex_code.data()), size);
        loaded_any = true;
    }
    
    // Load fragment shader
    std::string frag_path = path + ".frag.spv";
    std::ifstream frag_file(frag_path, std::ios::binary | std::ios::ate);
    if (frag_file.is_open()) {
        size_t size = frag_file.tellg();
        frag_file.seekg(0);
        m_fragment_code.resize(size / sizeof(u32));
        frag_file.read(reinterpret_cast<char*>(m_fragment_code.data()), size);
        loaded_any = true;
    }

    // Silently succeeding with empty SPIR-V vectors would cause downstream
    // Vulkan pipeline creation to crash.  Fail explicitly instead.
    if (!loaded_any) {
        LOG_ERROR("ShaderResource::Load(): no SPIR-V files found at '{}' "
                  "(looked for '{}' and '{}')",
                  path, vert_path, frag_path);
        SetState(ResourceState::Failed);
        return false;
    }
    
    SetState(ResourceState::Loaded);
    return true;
}

void ShaderResource::Unload() {
    m_vertex_code.clear();
    m_fragment_code.clear();
    m_pipeline = nullptr;
}

// ===== AudioResource =====

bool AudioResource::Load() {
    // Audio loading will be implemented with audio decoder
    SetState(ResourceState::Loaded);
    return true;
}

void AudioResource::Unload() {
    m_samples.clear();
    m_samples.shrink_to_fit();
}

} // namespace action
