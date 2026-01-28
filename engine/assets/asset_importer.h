#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "assets/asset_manager.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace action {

/*
 * ImportedMesh - Data from an imported mesh
 */
struct ImportedMesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    AABB bounds;
    
    // Material reference (by index in ImportedScene)
    i32 material_index = -1;
};

/*
 * ImportedMaterial - Material data from imported file
 */
struct ImportedMaterial {
    std::string name;
    vec3 diffuse_color{0.8f, 0.8f, 0.8f};
    vec3 specular_color{1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float opacity = 1.0f;
    
    // Texture paths (relative to model file)
    std::string diffuse_texture;
    std::string normal_texture;
    std::string roughness_texture;
    std::string metallic_texture;
};

/*
 * ImportedNode - Node hierarchy from imported file
 */
struct ImportedNode {
    std::string name;
    vec3 position{0, 0, 0};
    quat rotation = quat::identity();
    vec3 scale{1, 1, 1};
    
    // Mesh indices (a node can have multiple meshes)
    std::vector<i32> mesh_indices;
    
    // Child nodes
    std::vector<ImportedNode> children;
};

/*
 * ImportedScene - Complete imported 3D scene
 */
struct ImportedScene {
    std::string source_path;
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedMaterial> materials;
    ImportedNode root_node;
    
    // Bounding box of entire scene
    AABB scene_bounds;
    
    // Statistics
    u32 total_vertices = 0;
    u32 total_triangles = 0;
    u32 total_nodes = 0;
};

/*
 * ImportSettings - Configuration for import process
 */
struct ImportSettings {
    // Transform
    float scale = 1.0f;
    bool flip_uvs = true;          // Flip V coordinate for Vulkan
    bool flip_winding = false;     // Flip triangle winding
    bool generate_normals = true;  // Generate normals if missing
    bool generate_tangents = true; // Generate tangents for normal mapping
    
    // Optimization
    bool merge_meshes = false;     // Combine all meshes into one
    bool optimize_meshes = true;   // Optimize vertex cache
    
    // Import components
    bool import_materials = true;
    bool import_textures = true;
    bool import_animations = false; // Future: skeletal animations
    
    // Axis conversion (Blender uses Z-up, we use Y-up)
    enum class UpAxis { Y, Z };
    UpAxis source_up_axis = UpAxis::Z;
};

/*
 * ImportResult - Result of an import operation
 */
struct ImportResult {
    bool success = false;
    std::string error_message;
    ImportedScene scene;
    
    // Import statistics
    float import_time_ms = 0.0f;
};

/*
 * AssetImporter - Imports 3D assets from various formats
 * 
 * Supports:
 * - glTF 2.0 (.gltf, .glb)
 * - FBX (basic support via manual parsing)
 * - OBJ/MTL
 */
class AssetImporter {
public:
    AssetImporter() = default;
    ~AssetImporter() = default;
    
    // Initialize importer
    void Initialize(AssetManager* assets);
    
    // Import a 3D model file
    ImportResult Import(const std::string& filepath, const ImportSettings& settings = {});
    
    // Check if a file format is supported
    bool IsFormatSupported(const std::string& filepath) const;
    
    // Get supported file extensions
    std::vector<std::string> GetSupportedExtensions() const;
    
    // Convert imported scene to engine mesh handles
    std::vector<MeshHandle> CreateMeshes(const ImportedScene& scene, AssetManager& assets);
    
    // Progress callback for long imports
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;
    void SetProgressCallback(ProgressCallback callback) { m_progress_callback = callback; }
    
private:
    AssetManager* m_assets = nullptr;
    ProgressCallback m_progress_callback;
    
    // Format-specific importers
    ImportResult ImportGLTF(const std::string& filepath, const ImportSettings& settings);
    ImportResult ImportOBJ(const std::string& filepath, const ImportSettings& settings);
    ImportResult ImportFBX(const std::string& filepath, const ImportSettings& settings);
    
    // Helper functions
    void ApplyTransform(ImportedScene& scene, const ImportSettings& settings);
    void CalculateBounds(ImportedScene& scene);
    void GenerateNormals(ImportedMesh& mesh);
    void GenerateTangents(ImportedMesh& mesh);
    void FlipWindingOrder(ImportedMesh& mesh);
    void FlipUVs(ImportedMesh& mesh);
    
    // Report progress
    void ReportProgress(float progress, const std::string& status);
};

} // namespace action
