#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "assets/asset_manager.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward declarations for Assimp types
struct aiNode;
struct aiScene;
struct aiMesh;

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
 * ImportedBone - A single bone from a skeletal hierarchy
 */
struct ImportedBone {
    std::string name;
    i32 parent_index = -1;      // -1 = root bone
    mat4 offset_matrix;         // Inverse bind-pose (from Assimp aiBone::mOffsetMatrix)
    mat4 local_transform;       // Rest-pose local transform (from node hierarchy)
};

/*
 * ImportedSkeleton - Full bone hierarchy extracted from a skinned mesh
 */
struct ImportedSkeleton {
    std::string name;
    std::vector<ImportedBone> bones;
    bool is_valid() const { return !bones.empty(); }
};

/*
 * ImportedAnim* - Keyframe types for skeletal animation channels
 */
struct ImportedAnimKeyPos   { float time; vec3 value; };
struct ImportedAnimKeyRot   { float time; quat value; };
struct ImportedAnimKeyScale { float time; vec3 value; };

struct ImportedBoneChannel {
    std::string bone_name;
    std::vector<ImportedAnimKeyPos>   position_keys;
    std::vector<ImportedAnimKeyRot>   rotation_keys;
    std::vector<ImportedAnimKeyScale> scale_keys;
};

/*
 * ImportedAnimation - One animation clip (e.g. "Walk", "Run", "Attack")
 */
struct ImportedAnimation {
    std::string name;
    float duration = 0.0f;          // Duration in seconds
    float ticks_per_second = 24.0f;
    std::vector<ImportedBoneChannel> channels;
};

/*
 * ImportedVertexWeight / ImportedBoneWeights - Per-vertex skinning influence data
 */
struct ImportedVertexWeight { u32 vertex_index; float weight; };

struct ImportedBoneWeights {
    std::string bone_name;
    std::vector<ImportedVertexWeight> weights;
};

struct ImportedSkinnedMesh {
    i32 mesh_index = -1;                             // Index into ImportedScene::meshes
    std::vector<ImportedBoneWeights> bone_weights;   // Per-bone influence lists
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
    
    // Skeletal animation data (populated when import_animations == true)
    ImportedSkeleton skeleton;
    std::vector<ImportedAnimation> animations;
    std::vector<ImportedSkinnedMesh> skinned_meshes;
    
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
    bool generate_tangents = false; // Generate tangents for normal mapping (slow)
    
    // Optimization
    bool merge_meshes = false;     // Combine all meshes into one
    bool optimize_meshes = false;  // Optimize vertex cache (slow for large models)
    bool fast_import = true;       // Skip expensive post-processing for speed
    
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
    
    // Assimp-based universal importer
    ImportResult ImportWithAssimp(const std::string& filepath, const ImportSettings& settings);
    ImportedMesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const ImportSettings& settings);
    void ProcessNode(aiNode* node, const aiScene* scene, ImportedNode& out_node);
    void ExtractMaterials(const aiScene* scene, ImportedScene& out_scene, const std::string& model_path);
    u32 CountNodes(const ImportedNode& node);
    
    // Sidecar (.aeimport) loading — overrides ImportSettings before import
    void LoadSidecar(const std::string& filepath, ImportSettings& settings);
    
    // Skeletal animation extraction
    void ExtractSkeleton(const aiScene* scene, ImportedScene& out_scene);
    void ExtractAnimations(const aiScene* scene, ImportedScene& out_scene);
    void ExtractSkinnedMeshes(const aiScene* scene, ImportedScene& out_scene);
    
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
