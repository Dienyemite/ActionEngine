#include "asset_importer.h"
#include "core/logging.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace action {

void AssetImporter::Initialize(AssetManager* assets) {
    m_assets = assets;
    LOG_INFO("AssetImporter initialized");
}

bool AssetImporter::IsFormatSupported(const std::string& filepath) const {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == "obj" || ext == "gltf" || ext == "glb" || ext == "fbx";
}

std::vector<std::string> AssetImporter::GetSupportedExtensions() const {
    return {".obj", ".gltf", ".glb", ".fbx"};
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
    
    // Determine format and import
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    ReportProgress(0.0f, "Loading " + filepath);
    
    if (ext == "obj") {
        result = ImportOBJ(filepath, settings);
    } else if (ext == "gltf" || ext == "glb") {
        result = ImportGLTF(filepath, settings);
    } else if (ext == "fbx") {
        result = ImportFBX(filepath, settings);
    } else {
        result.success = false;
        result.error_message = "Unsupported format: " + ext;
        LOG_ERROR("{}", result.error_message);
        return result;
    }
    
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
// OBJ Importer (simple, no dependencies)
// ============================================================================

ImportResult AssetImporter::ImportOBJ(const std::string& filepath, const ImportSettings& settings) {
    ImportResult result;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.success = false;
        result.error_message = "Failed to open file: " + filepath;
        return result;
    }
    
    std::vector<vec3> positions;
    std::vector<vec3> normals;
    std::vector<vec2> texcoords;
    
    ImportedMesh current_mesh;
    current_mesh.name = "Mesh";
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        if (prefix == "v") {
            vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (prefix == "vn") {
            vec3 norm;
            iss >> norm.x >> norm.y >> norm.z;
            normals.push_back(norm);
        }
        else if (prefix == "vt") {
            vec2 tex;
            iss >> tex.x >> tex.y;
            texcoords.push_back(tex);
        }
        else if (prefix == "f") {
            // Parse face - can be v, v/vt, v/vt/vn, or v//vn
            std::vector<Vertex> face_verts;
            std::string vertex_str;
            
            while (iss >> vertex_str) {
                Vertex vert;
                vert.color = vec3{1, 1, 1};
                
                // Parse v/vt/vn format
                size_t pos1 = vertex_str.find('/');
                size_t pos2 = vertex_str.find('/', pos1 + 1);
                
                int vi = 0, ti = 0, ni = 0;
                
                if (pos1 == std::string::npos) {
                    // Just position
                    vi = std::stoi(vertex_str);
                } else if (pos2 == std::string::npos) {
                    // v/vt
                    vi = std::stoi(vertex_str.substr(0, pos1));
                    if (pos1 + 1 < vertex_str.length()) {
                        ti = std::stoi(vertex_str.substr(pos1 + 1));
                    }
                } else {
                    // v/vt/vn or v//vn
                    vi = std::stoi(vertex_str.substr(0, pos1));
                    if (pos2 > pos1 + 1) {
                        ti = std::stoi(vertex_str.substr(pos1 + 1, pos2 - pos1 - 1));
                    }
                    if (pos2 + 1 < vertex_str.length()) {
                        ni = std::stoi(vertex_str.substr(pos2 + 1));
                    }
                }
                
                // OBJ indices are 1-based
                if (vi > 0 && vi <= (int)positions.size()) {
                    vert.position = positions[vi - 1];
                }
                if (ti > 0 && ti <= (int)texcoords.size()) {
                    vert.uv = texcoords[ti - 1];
                }
                if (ni > 0 && ni <= (int)normals.size()) {
                    vert.normal = normals[ni - 1];
                }
                
                face_verts.push_back(vert);
            }
            
            // Triangulate the face (assuming convex polygon)
            for (size_t i = 2; i < face_verts.size(); i++) {
                u32 base_index = (u32)current_mesh.vertices.size();
                current_mesh.vertices.push_back(face_verts[0]);
                current_mesh.vertices.push_back(face_verts[i - 1]);
                current_mesh.vertices.push_back(face_verts[i]);
                current_mesh.indices.push_back(base_index);
                current_mesh.indices.push_back(base_index + 1);
                current_mesh.indices.push_back(base_index + 2);
            }
        }
        else if (prefix == "o" || prefix == "g") {
            // Object or group name
            std::string name;
            iss >> name;
            if (!current_mesh.vertices.empty()) {
                // Save current mesh and start new one
                result.scene.meshes.push_back(current_mesh);
                current_mesh = ImportedMesh();
            }
            current_mesh.name = name;
        }
    }
    
    // Add the last mesh
    if (!current_mesh.vertices.empty()) {
        result.scene.meshes.push_back(current_mesh);
    }
    
    // Generate normals if needed
    if (settings.generate_normals && normals.empty()) {
        for (auto& mesh : result.scene.meshes) {
            GenerateNormals(mesh);
        }
    }
    
    // Apply settings
    if (settings.flip_uvs) {
        for (auto& mesh : result.scene.meshes) {
            FlipUVs(mesh);
        }
    }
    
    if (settings.flip_winding) {
        for (auto& mesh : result.scene.meshes) {
            FlipWindingOrder(mesh);
        }
    }
    
    // Calculate statistics
    for (const auto& mesh : result.scene.meshes) {
        result.scene.total_vertices += (u32)mesh.vertices.size();
        result.scene.total_triangles += (u32)mesh.indices.size() / 3;
    }
    
    // Create root node
    result.scene.root_node.name = "Root";
    for (size_t i = 0; i < result.scene.meshes.size(); i++) {
        result.scene.root_node.mesh_indices.push_back((i32)i);
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// glTF Importer (basic implementation)
// ============================================================================

ImportResult AssetImporter::ImportGLTF(const std::string& filepath, const ImportSettings& settings) {
    ImportResult result;
    
    // TODO: Implement proper glTF parsing
    // For now, just log and return error
    // A full implementation would parse JSON and binary data
    
    result.success = false;
    result.error_message = "glTF import not yet implemented - use OBJ for now";
    LOG_WARN("{}", result.error_message);
    
    return result;
}

// ============================================================================
// FBX Importer (basic implementation)
// ============================================================================

ImportResult AssetImporter::ImportFBX(const std::string& filepath, const ImportSettings& settings) {
    ImportResult result;
    
    // TODO: Implement FBX parsing
    // FBX is a complex binary format, would need a library like OpenFBX
    
    result.success = false;
    result.error_message = "FBX import not yet implemented - use OBJ for now";
    LOG_WARN("{}", result.error_message);
    
    return result;
}

// ============================================================================
// Helper Functions
// ============================================================================

void AssetImporter::ApplyTransform(ImportedScene& scene, const ImportSettings& settings) {
    // Apply scale
    if (settings.scale != 1.0f) {
        for (auto& mesh : scene.meshes) {
            for (auto& vert : mesh.vertices) {
                vert.position.x *= settings.scale;
                vert.position.y *= settings.scale;
                vert.position.z *= settings.scale;
            }
        }
    }
    
    // Apply axis conversion (Z-up to Y-up)
    if (settings.source_up_axis == ImportSettings::UpAxis::Z) {
        for (auto& mesh : scene.meshes) {
            for (auto& vert : mesh.vertices) {
                // Rotate -90 degrees around X axis
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
    // Calculate face normals and average them per vertex
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
    
    // Normalize
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        mesh.vertices[i].normal = vertex_normals[i].normalized();
    }
}

void AssetImporter::GenerateTangents(ImportedMesh& mesh) {
    // TODO: Implement Mikktspace tangent generation
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
