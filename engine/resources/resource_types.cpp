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
    m_pixel_data.clear();
    m_pixel_data.shrink_to_fit();
    m_gpu_handle = nullptr;
}

// ===== MeshResource =====

bool MeshResource::Load() {
    // Actual mesh loading will be implemented with OBJ/GLTF loader
    SetState(ResourceState::Loaded);
    return true;
}

void MeshResource::Unload() {
    m_vertices.clear();
    m_vertices.shrink_to_fit();
    m_indices.clear();
    m_indices.shrink_to_fit();
    m_vertex_buffer = nullptr;
    m_index_buffer = nullptr;
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
    
    // Load vertex shader
    std::string vert_path = path + ".vert.spv";
    std::ifstream vert_file(vert_path, std::ios::binary | std::ios::ate);
    if (vert_file.is_open()) {
        size_t size = vert_file.tellg();
        vert_file.seekg(0);
        m_vertex_code.resize(size / sizeof(u32));
        vert_file.read(reinterpret_cast<char*>(m_vertex_code.data()), size);
    }
    
    // Load fragment shader
    std::string frag_path = path + ".frag.spv";
    std::ifstream frag_file(frag_path, std::ios::binary | std::ios::ate);
    if (frag_file.is_open()) {
        size_t size = frag_file.tellg();
        frag_file.seekg(0);
        m_fragment_code.resize(size / sizeof(u32));
        frag_file.read(reinterpret_cast<char*>(m_fragment_code.data()), size);
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
