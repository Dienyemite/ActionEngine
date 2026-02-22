#pragma once

#include "resource.h"
#include "core/types.h"
#include <vector>

namespace action {

/*
 * Common Resource Types (Godot-style)
 * 
 * These are the fundamental resource types used throughout the engine.
 */

// ===== Texture Resource =====

// Callback type used by the renderer to destroy its GPU-side objects.
// Set via SetGPUReleaseCallback() when the renderer uploads GPU resources.
using GPUReleaseFunc = std::function<void(void* handle)>;

class TextureResource : public Resource {
public:
    TextureResource() = default;
    explicit TextureResource(const std::string& path) : Resource(path) {}
    
    std::string GetTypeName() const override { return "Texture"; }
    static std::string GetStaticTypeName() { return "Texture"; }
    
    // Texture properties
    u32 GetWidth() const { return m_width; }
    u32 GetHeight() const { return m_height; }
    u32 GetMipLevels() const { return m_mip_levels; }
    u32 GetFormat() const { return m_format; }
    
    void SetDimensions(u32 width, u32 height) { m_width = width; m_height = height; }
    void SetMipLevels(u32 levels) { m_mip_levels = levels; }
    void SetFormat(u32 format) { m_format = format; }
    
    // Pixel data
    const std::vector<u8>& GetPixelData() const { return m_pixel_data; }
    void SetPixelData(std::vector<u8> data) { m_pixel_data = std::move(data); }
    
    // GPU handle (set by renderer after upload)
    void* GetGPUHandle() const { return m_gpu_handle; }
    void SetGPUHandle(void* handle) { m_gpu_handle = handle; }
    
    // Register a callback the renderer uses to destroy m_gpu_handle on Unload.
    // Must be called by the renderer whenever it populates the GPU handle.
    void SetGPUReleaseCallback(GPUReleaseFunc func) { m_gpu_release_func = std::move(func); }
    
    size_t GetMemoryUsage() const override {
        return sizeof(*this) + m_pixel_data.size();
    }
    
    bool Load() override;
    void Unload() override;
    
private:
    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_mip_levels = 1;
    u32 m_format = 0;  // VkFormat
    std::vector<u8> m_pixel_data;
    void* m_gpu_handle = nullptr;
    GPUReleaseFunc m_gpu_release_func;  // Destroys m_gpu_handle (set by renderer)
};

// ===== Mesh Resource =====
struct MeshVertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 color;
};

class MeshResource : public Resource {
public:
    MeshResource() = default;
    explicit MeshResource(const std::string& path) : Resource(path) {}
    
    std::string GetTypeName() const override { return "Mesh"; }
    static std::string GetStaticTypeName() { return "Mesh"; }
    
    // Mesh data
    const std::vector<MeshVertex>& GetVertices() const { return m_vertices; }
    const std::vector<u32>& GetIndices() const { return m_indices; }
    
    void SetVertices(std::vector<MeshVertex> vertices) { m_vertices = std::move(vertices); }
    void SetIndices(std::vector<u32> indices) { m_indices = std::move(indices); }
    
    u32 GetVertexCount() const { return static_cast<u32>(m_vertices.size()); }
    u32 GetIndexCount() const { return static_cast<u32>(m_indices.size()); }
    u32 GetTriangleCount() const { return GetIndexCount() / 3; }
    
    // Bounds
    const AABB& GetBounds() const { return m_bounds; }
    void CalculateBounds();
    
    // GPU handles
    void* GetVertexBuffer() const { return m_vertex_buffer; }
    void* GetIndexBuffer() const { return m_index_buffer; }
    void SetGPUBuffers(void* vertex, void* index) { m_vertex_buffer = vertex; m_index_buffer = index; }
    
    // Register callbacks so Unload() can destroy the GPU buffers without leaking.
    void SetVertexReleaseCallback(GPUReleaseFunc func) { m_vb_release_func = std::move(func); }
    void SetIndexReleaseCallback (GPUReleaseFunc func) { m_ib_release_func = std::move(func); }
    
    size_t GetMemoryUsage() const override {
        return sizeof(*this) + m_vertices.size() * sizeof(MeshVertex) + m_indices.size() * sizeof(u32);
    }
    
    bool Load() override;
    void Unload() override;
    
private:
    std::vector<MeshVertex> m_vertices;
    std::vector<u32> m_indices;
    AABB m_bounds;
    void* m_vertex_buffer = nullptr;
    void* m_index_buffer  = nullptr;
    GPUReleaseFunc m_vb_release_func;  // Destroys m_vertex_buffer (set by renderer)
    GPUReleaseFunc m_ib_release_func;  // Destroys m_index_buffer  (set by renderer)
};

// ===== Material Resource =====
class MaterialResource : public Resource {
public:
    MaterialResource() = default;
    explicit MaterialResource(const std::string& path) : Resource(path) {}
    
    std::string GetTypeName() const override { return "Material"; }
    static std::string GetStaticTypeName() { return "Material"; }
    
    // Textures
    Ref<TextureResource> GetAlbedoTexture() const { return m_albedo; }
    Ref<TextureResource> GetNormalTexture() const { return m_normal; }
    Ref<TextureResource> GetMetallicRoughnessTexture() const { return m_metallic_roughness; }
    
    void SetAlbedoTexture(Ref<TextureResource> tex) { m_albedo = tex; }
    void SetNormalTexture(Ref<TextureResource> tex) { m_normal = tex; }
    void SetMetallicRoughnessTexture(Ref<TextureResource> tex) { m_metallic_roughness = tex; }
    
    // Properties
    vec4 GetAlbedoColor() const { return m_albedo_color; }
    float GetMetallic() const { return m_metallic; }
    float GetRoughness() const { return m_roughness; }
    float GetEmissionStrength() const { return m_emission_strength; }
    
    void SetAlbedoColor(const vec4& color) { m_albedo_color = color; }
    void SetMetallic(float v) { m_metallic = v; }
    void SetRoughness(float v) { m_roughness = v; }
    void SetEmissionStrength(float v) { m_emission_strength = v; }
    
    // Rendering mode
    bool IsTransparent() const { return m_transparent; }
    bool IsDoubleSided() const { return m_double_sided; }
    void SetTransparent(bool t) { m_transparent = t; }
    void SetDoubleSided(bool d) { m_double_sided = d; }
    
    bool Load() override;
    void Unload() override;
    
private:
    Ref<TextureResource> m_albedo;
    Ref<TextureResource> m_normal;
    Ref<TextureResource> m_metallic_roughness;
    
    vec4 m_albedo_color{1, 1, 1, 1};
    float m_metallic = 0.0f;
    float m_roughness = 0.5f;
    float m_emission_strength = 0.0f;
    
    bool m_transparent = false;
    bool m_double_sided = false;
};

// ===== Shader Resource =====
class ShaderResource : public Resource {
public:
    ShaderResource() = default;
    explicit ShaderResource(const std::string& path) : Resource(path) {}
    
    std::string GetTypeName() const override { return "Shader"; }
    static std::string GetStaticTypeName() { return "Shader"; }
    
    // Shader code (SPIR-V binary)
    const std::vector<u32>& GetVertexCode() const { return m_vertex_code; }
    const std::vector<u32>& GetFragmentCode() const { return m_fragment_code; }
    
    void SetVertexCode(std::vector<u32> code) { m_vertex_code = std::move(code); }
    void SetFragmentCode(std::vector<u32> code) { m_fragment_code = std::move(code); }
    
    // GPU handle (VkPipeline)
    void* GetPipelineHandle() const { return m_pipeline; }
    void SetPipelineHandle(void* handle) { m_pipeline = handle; }
    
    bool Load() override;
    void Unload() override;
    
private:
    std::vector<u32> m_vertex_code;
    std::vector<u32> m_fragment_code;
    void* m_pipeline = nullptr;
};

// ===== Audio Resource =====
class AudioResource : public Resource {
public:
    AudioResource() = default;
    explicit AudioResource(const std::string& path) : Resource(path) {}
    
    std::string GetTypeName() const override { return "Audio"; }
    static std::string GetStaticTypeName() { return "Audio"; }
    
    // Audio properties
    u32 GetSampleRate() const { return m_sample_rate; }
    u32 GetChannels() const { return m_channels; }
    float GetDuration() const { return m_duration; }
    
    void SetSampleRate(u32 rate) { m_sample_rate = rate; }
    void SetChannels(u32 channels) { m_channels = channels; }
    void SetDuration(float duration) { m_duration = duration; }
    
    // Audio data
    const std::vector<i16>& GetSamples() const { return m_samples; }
    void SetSamples(std::vector<i16> samples) { m_samples = std::move(samples); }
    
    size_t GetMemoryUsage() const override {
        return sizeof(*this) + m_samples.size() * sizeof(i16);
    }
    
    bool Load() override;
    void Unload() override;
    
private:
    u32 m_sample_rate = 44100;
    u32 m_channels = 2;
    float m_duration = 0;
    std::vector<i16> m_samples;
};

} // namespace action
