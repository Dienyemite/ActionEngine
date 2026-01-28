#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace action {

// Forward declarations
class Editor;
struct EditorNode;
class AssetManager;

/*
 * PrefabNodeData - Serializable node data for prefabs
 * 
 * Contains all the data needed to recreate a node
 */
struct PrefabNodeData {
    std::string name;
    std::string type;
    vec3 position{0, 0, 0};
    vec3 rotation{0, 0, 0};
    vec3 scale{1, 1, 1};
    vec3 color{0.8f, 0.8f, 0.8f};
    bool visible = true;
    
    // Children nodes (recursive structure)
    std::vector<PrefabNodeData> children;
};

/*
 * Prefab - A reusable node template
 * 
 * Prefabs are saved node hierarchies that can be instantiated
 * multiple times. Changes to a prefab can optionally propagate
 * to all instances.
 */
class Prefab {
public:
    Prefab() = default;
    Prefab(const std::string& name);
    ~Prefab() = default;
    
    // Name
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }
    
    // File path (for saved prefabs)
    const std::string& GetPath() const { return m_path; }
    void SetPath(const std::string& path) { m_path = path; }
    
    // Root node data
    const PrefabNodeData& GetRootData() const { return m_root_data; }
    PrefabNodeData& GetRootData() { return m_root_data; }
    
    // Create prefab from editor node
    void CaptureFromNode(const EditorNode& node);
    
    // Serialize/deserialize
    bool SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);
    
    // JSON serialization helpers
    std::string ToJson() const;
    bool FromJson(const std::string& json);
    
private:
    std::string m_name;
    std::string m_path;
    PrefabNodeData m_root_data;
    
    // Recursively capture node data
    void CaptureNodeRecursive(const EditorNode& node, PrefabNodeData& data);
};

/*
 * PrefabManager - Manages loaded prefabs and instances
 */
class PrefabManager {
public:
    PrefabManager() = default;
    ~PrefabManager() = default;
    
    // Initialize with reference to editor and asset systems
    void Initialize(Editor* editor, AssetManager* assets);
    
    // Create a new prefab from a node
    std::shared_ptr<Prefab> CreatePrefab(const std::string& name, const EditorNode& node);
    
    // Load a prefab from file
    std::shared_ptr<Prefab> LoadPrefab(const std::string& path);
    
    // Save a prefab to file
    bool SavePrefab(std::shared_ptr<Prefab> prefab, const std::string& path);
    
    // Get a prefab by name
    std::shared_ptr<Prefab> GetPrefab(const std::string& name);
    
    // Get all loaded prefabs
    const std::unordered_map<std::string, std::shared_ptr<Prefab>>& GetAllPrefabs() const {
        return m_prefabs;
    }
    
    // Instantiate a prefab into the scene
    // Returns the root node ID of the new instance
    u32 InstantiatePrefab(std::shared_ptr<Prefab> prefab, EditorNode* parent = nullptr);
    
    // Unload a prefab
    void UnloadPrefab(const std::string& name);
    
    // Clear all prefabs
    void Clear();
    
    // Directory for prefabs
    void SetPrefabDirectory(const std::string& dir) { m_prefab_directory = dir; }
    const std::string& GetPrefabDirectory() const { return m_prefab_directory; }
    
    // Scan directory for prefab files
    void ScanPrefabDirectory();
    
private:
    Editor* m_editor = nullptr;
    AssetManager* m_assets = nullptr;
    std::string m_prefab_directory = "prefabs";
    std::unordered_map<std::string, std::shared_ptr<Prefab>> m_prefabs;
    
    // Recursively instantiate node data
    void InstantiateNodeRecursive(const PrefabNodeData& data, EditorNode* parent);
};

} // namespace action
