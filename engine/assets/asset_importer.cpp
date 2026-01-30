#include "asset_importer.h"
#include "core/logging.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace action {

void AssetImporter::Initialize(AssetManager* assets) {
    m_assets = assets;
    LOG_INFO("AssetImporter initialized");
    LOG_INFO("  Supported formats: .obj, .fbx, .gltf, .glb, .blend, .dae, .3ds, .stl, .ply");
}

bool AssetImporter::IsFormatSupported(const std::string& filepath) const {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Formats enabled in our Assimp build
    return ext == "obj" || ext == "fbx" || ext == "gltf" || ext == "glb" || 
           ext == "blend" || ext == "dae" || ext == "3ds" || ext == "stl" || ext == "ply";
}

std::vector<std::string> AssetImporter::GetSupportedExtensions() const {
    return {".obj", ".fbx", ".gltf", ".glb", ".blend", ".dae", ".3ds", ".stl", ".ply"};
}

ImportResult AssetImporter::Import(const std::string& filepath, const ImportSettings& settings) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    ImportResult result;
    
    // Check if file exists
    std::ifstream file(filepath);
    if (!file.good()) {
        result.success = false;
        result.error_message = "File not found: " + filepath;
        LOG_ERROR("{}", result.error_message);
        return result;
    }
    file.close();
    
    ReportProgress(0.0f, "Loading " + filepath);
    
    // Use Assimp for all formats
    result = ImportWithAssimp(filepath, settings);
    
    if (result.success) {
        // Apply post-processing
        ReportProgress(0.8f, "Processing...");
        ApplyTransform(result.scene, settings);
        CalculateBounds(result.scene);
        
        result.scene.source_path = filepath;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.import_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
        
        LOG_INFO("Imported '{}': {} meshes, {} materials, {} verts, {} tris in {:.1f}ms",
                 filepath, result.scene.meshes.size(), result.scene.materials.size(),
                 result.scene.total_vertices, result.scene.total_triangles,
                 result.import_time_ms);
    }
    
    ReportProgress(1.0f, "Done");
    return result;
}

// ============================================================================
// Assimp Importer - Universal format support
// ============================================================================

ImportResult AssetImporter::ImportWithAssimp(const std::string& filepath, const ImportSettings& settings) {
    ImportResult result;
    
    Assimp::Importer importer;
    
    // Configure import flags - minimal set for fast loading
    unsigned int flags = aiProcess_Triangulate;  // Always triangulate
    
    if (!settings.fast_import) {
        // Full processing (slower but higher quality)
        if (settings.generate_normals) {
            flags |= aiProcess_GenSmoothNormals;
        }
        if (settings.generate_tangents) {
            flags |= aiProcess_CalcTangentSpace;
        }
        if (settings.optimize_meshes) {
            flags |= aiProcess_JoinIdenticalVertices;
            flags |= aiProcess_OptimizeMeshes;
            flags |= aiProcess_RemoveRedundantMaterials;
        }
        flags |= aiProcess_ValidateDataStructure;
    } else {
        // Fast import - only essential processing
        flags |= aiProcess_GenNormals;  // Fast normal generation (not smooth)
    }
    
    // These are fast operations, always apply
    if (settings.flip_uvs) {
        flags |= aiProcess_FlipUVs;
    }
    if (settings.flip_winding) {
        flags |= aiProcess_FlipWindingOrder;
    }
    
    // Apply axis conversion if needed (Blender Z-up to Y-up)
    if (settings.source_up_axis == ImportSettings::UpAxis::Z) {
        flags |= aiProcess_MakeLeftHanded;  // This helps with Z-up to Y-up
    }
    
    ReportProgress(0.2f, "Parsing file...");
    
    const aiScene* scene = importer.ReadFile(filepath, flags);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        result.success = false;
        result.error_message = "Assimp error: " + std::string(importer.GetErrorString());
        LOG_ERROR("{}", result.error_message);
        return result;
    }
    
    ReportProgress(0.4f, "Processing meshes...");
    
    // Extract materials first (meshes reference them by index)
    if (settings.import_materials) {
        ExtractMaterials(scene, result.scene, filepath);
    }
    
    ReportProgress(0.5f, "Extracting geometry...");
    
    // Extract all meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        ImportedMesh imported_mesh = ProcessMesh(mesh, scene, settings);
        
        // Apply scale if set
        if (settings.scale != 1.0f) {
            for (auto& vert : imported_mesh.vertices) {
                vert.position.x *= settings.scale;
                vert.position.y *= settings.scale;
                vert.position.z *= settings.scale;
            }
        }
        
        result.scene.meshes.push_back(imported_mesh);
        result.scene.total_vertices += (u32)imported_mesh.vertices.size();
        result.scene.total_triangles += (u32)imported_mesh.indices.size() / 3;
    }
    
    ReportProgress(0.7f, "Building node hierarchy...");
    
    // Process node hierarchy
    ProcessNode(scene->mRootNode, scene, result.scene.root_node);
    result.scene.total_nodes = CountNodes(result.scene.root_node);
    
    result.success = true;
    return result;
}

ImportedMesh AssetImporter::ProcessMesh(aiMesh* mesh, const aiScene* scene, const ImportSettings& settings) {
    ImportedMesh imported_mesh;
    imported_mesh.name = mesh->mName.C_Str();
    imported_mesh.material_index = mesh->mMaterialIndex;
    
    // Reserve space
    imported_mesh.vertices.reserve(mesh->mNumVertices);
    imported_mesh.indices.reserve(mesh->mNumFaces * 3);
    
    // Extract vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vert;
        
        // Position
        vert.position.x = mesh->mVertices[i].x;
        vert.position.y = mesh->mVertices[i].y;
        vert.position.z = mesh->mVertices[i].z;
        
        // Normal
        if (mesh->HasNormals()) {
            vert.normal.x = mesh->mNormals[i].x;
            vert.normal.y = mesh->mNormals[i].y;
            vert.normal.z = mesh->mNormals[i].z;
        } else {
            vert.normal = vec3{0, 1, 0};
        }
        
        // UV coordinates (use first UV channel)
        if (mesh->mTextureCoords[0]) {
            vert.uv.x = mesh->mTextureCoords[0][i].x;
            vert.uv.y = mesh->mTextureCoords[0][i].y;
        } else {
            vert.uv = vec2{0, 0};
        }
        
        // Vertex color (use first color channel)
        if (mesh->mColors[0]) {
            vert.color.x = mesh->mColors[0][i].r;
            vert.color.y = mesh->mColors[0][i].g;
            vert.color.z = mesh->mColors[0][i].b;
        } else {
            vert.color = vec3{1, 1, 1};
        }
        
        imported_mesh.vertices.push_back(vert);
    }
    
    // Extract indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace& face = mesh->mFaces[i];
        // Should always be triangles due to aiProcess_Triangulate
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            imported_mesh.indices.push_back(face.mIndices[j]);
        }
    }
    
    return imported_mesh;
}

void AssetImporter::ProcessNode(aiNode* node, const aiScene* scene, ImportedNode& out_node) {
    out_node.name = node->mName.C_Str();
    
    // Extract transform
    aiMatrix4x4& m = node->mTransformation;
    aiVector3D scaling, position;
    aiQuaternion rotation;
    m.Decompose(scaling, rotation, position);
    
    out_node.position = vec3{position.x, position.y, position.z};
    out_node.rotation = quat{rotation.x, rotation.y, rotation.z, rotation.w};
    out_node.scale = vec3{scaling.x, scaling.y, scaling.z};
    
    // Add mesh references
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        out_node.mesh_indices.push_back((i32)node->mMeshes[i]);
    }
    
    // Process children
    out_node.children.resize(node->mNumChildren);
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, out_node.children[i]);
    }
}

void AssetImporter::ExtractMaterials(const aiScene* scene, ImportedScene& out_scene, const std::string& model_path) {
    std::filesystem::path model_dir = std::filesystem::path(model_path).parent_path();
    
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];
        ImportedMaterial imported_mat;
        
        // Material name
        aiString name;
        if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            imported_mat.name = name.C_Str();
        } else {
            imported_mat.name = "Material_" + std::to_string(i);
        }
        
        // Diffuse color
        aiColor4D diffuse;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
            imported_mat.diffuse_color = vec3{diffuse.r, diffuse.g, diffuse.b};
        }
        
        // Specular color
        aiColor4D specular;
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS) {
            imported_mat.specular_color = vec3{specular.r, specular.g, specular.b};
        }
        
        // Roughness/Shininess
        float shininess;
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            // Convert shininess to roughness (inverse relationship)
            imported_mat.roughness = 1.0f - std::min(shininess / 1000.0f, 1.0f);
        }
        
        // Opacity
        float opacity;
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
            imported_mat.opacity = opacity;
        }
        
        // Diffuse texture
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            imported_mat.diffuse_texture = (model_dir / texPath.C_Str()).string();
        }
        
        // Normal map
        if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
            aiString texPath;
            material->GetTexture(aiTextureType_NORMALS, 0, &texPath);
            imported_mat.normal_texture = (model_dir / texPath.C_Str()).string();
        } else if (material->GetTextureCount(aiTextureType_HEIGHT) > 0) {
            // Some formats use HEIGHT for normal maps
            aiString texPath;
            material->GetTexture(aiTextureType_HEIGHT, 0, &texPath);
            imported_mat.normal_texture = (model_dir / texPath.C_Str()).string();
        }
        
        // PBR metallic-roughness (glTF 2.0 style)
        float metallic;
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
            imported_mat.metallic = metallic;
        }
        
        float roughness;
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
            imported_mat.roughness = roughness;
        }
        
        out_scene.materials.push_back(imported_mat);
    }
}

u32 AssetImporter::CountNodes(const ImportedNode& node) {
    u32 count = 1;
    for (const auto& child : node.children) {
        count += CountNodes(child);
    }
    return count;
}

// ============================================================================
// Legacy importers - now redirect to Assimp
// ============================================================================

ImportResult AssetImporter::ImportOBJ(const std::string& filepath, const ImportSettings& settings) {
    return ImportWithAssimp(filepath, settings);
}

ImportResult AssetImporter::ImportGLTF(const std::string& filepath, const ImportSettings& settings) {
    return ImportWithAssimp(filepath, settings);
}

ImportResult AssetImporter::ImportFBX(const std::string& filepath, const ImportSettings& settings) {
    return ImportWithAssimp(filepath, settings);
}

// ============================================================================
// Helper Functions
// ============================================================================

void AssetImporter::ApplyTransform(ImportedScene& scene, const ImportSettings& settings) {
    // Note: Scale and axis conversion now handled by Assimp or ProcessMesh
    // This function kept for any additional post-processing needed
    
    // Apply axis conversion (Z-up to Y-up) if Assimp didn't fully handle it
    if (settings.source_up_axis == ImportSettings::UpAxis::Z) {
        for (auto& mesh : scene.meshes) {
            for (auto& vert : mesh.vertices) {
                // Rotate -90 degrees around X axis (swap Y and Z, negate new Z)
                float y = vert.position.y;
                float z = vert.position.z;
                vert.position.y = z;
                vert.position.z = -y;
                
                // Also rotate normals
                y = vert.normal.y;
                z = vert.normal.z;
                vert.normal.y = z;
                vert.normal.z = -y;
            }
        }
    }
}

void AssetImporter::CalculateBounds(ImportedScene& scene) {
    scene.scene_bounds = AABB();
    
    for (auto& mesh : scene.meshes) {
        mesh.bounds = AABB();
        for (const auto& vert : mesh.vertices) {
            mesh.bounds.expand(vert.position);
        }
        scene.scene_bounds.expand(mesh.bounds.min);
        scene.scene_bounds.expand(mesh.bounds.max);
    }
}

void AssetImporter::GenerateNormals(ImportedMesh& mesh) {
    // Note: Assimp handles this with aiProcess_GenSmoothNormals
    std::vector<vec3> vertex_normals(mesh.vertices.size(), vec3{0, 0, 0});
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        u32 i0 = mesh.indices[i];
        u32 i1 = mesh.indices[i + 1];
        u32 i2 = mesh.indices[i + 2];
        
        vec3 p0 = mesh.vertices[i0].position;
        vec3 p1 = mesh.vertices[i1].position;
        vec3 p2 = mesh.vertices[i2].position;
        
        vec3 edge1 = p1 - p0;
        vec3 edge2 = p2 - p0;
        vec3 face_normal = edge1.cross(edge2);
        
        vertex_normals[i0] = vertex_normals[i0] + face_normal;
        vertex_normals[i1] = vertex_normals[i1] + face_normal;
        vertex_normals[i2] = vertex_normals[i2] + face_normal;
    }
    
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        mesh.vertices[i].normal = vertex_normals[i].normalized();
    }
}

void AssetImporter::GenerateTangents(ImportedMesh& mesh) {
    // Note: Assimp handles this with aiProcess_CalcTangentSpace
}

void AssetImporter::FlipWindingOrder(ImportedMesh& mesh) {
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    }
}

void AssetImporter::FlipUVs(ImportedMesh& mesh) {
    for (auto& vert : mesh.vertices) {
        vert.uv.y = 1.0f - vert.uv.y;
    }
}

void AssetImporter::ReportProgress(float progress, const std::string& status) {
    if (m_progress_callback) {
        m_progress_callback(progress, status);
    }
}

std::vector<MeshHandle> AssetImporter::CreateMeshes(const ImportedScene& scene, AssetManager& assets) {
    std::vector<MeshHandle> handles;
    
    for (const auto& imported_mesh : scene.meshes) {
        MeshData mesh_data;
        mesh_data.name = imported_mesh.name;
        mesh_data.vertices = imported_mesh.vertices;
        mesh_data.indices = imported_mesh.indices;
        mesh_data.bounds = imported_mesh.bounds;
        
        MeshHandle handle = assets.CreateMesh(mesh_data);
        if (handle.is_valid()) {
            handles.push_back(handle);
        }
    }
    
    return handles;
}

} // namespace action
