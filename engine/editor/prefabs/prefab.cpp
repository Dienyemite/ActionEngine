#include "prefab.h"
#include "editor/editor.h"
#include "core/logging.h"
#include <fstream>
#include <sstream>

namespace action {

// ============================================================================
// Prefab
// ============================================================================

Prefab::Prefab(const std::string& name)
    : m_name(name)
{
}

void Prefab::CaptureFromNode(const EditorNode& node) {
    CaptureNodeRecursive(node, m_root_data);
    if (m_name.empty()) {
        m_name = node.name;
    }
}

void Prefab::CaptureNodeRecursive(const EditorNode& node, PrefabNodeData& data) {
    data.name = node.name;
    data.type = node.type;
    data.position = node.position;
    data.rotation = node.rotation;
    data.scale = node.scale;
    data.color = node.color;
    data.visible = node.visible;
    
    // Capture children
    data.children.clear();
    for (const auto& child : node.children) {
        PrefabNodeData child_data;
        CaptureNodeRecursive(child, child_data);
        data.children.push_back(child_data);
    }
}

std::string Prefab::ToJson() const {
    // Simple JSON serialization (manual, avoiding external dependencies)
    std::ostringstream oss;
    
    std::function<void(const PrefabNodeData&, int)> write_node;
    write_node = [&oss, &write_node](const PrefabNodeData& node, int indent) {
        std::string ind(indent * 2, ' ');
        oss << ind << "{\n";
        oss << ind << "  \"name\": \"" << node.name << "\",\n";
        oss << ind << "  \"type\": \"" << node.type << "\",\n";
        oss << ind << "  \"position\": [" << node.position.x << ", " << node.position.y << ", " << node.position.z << "],\n";
        oss << ind << "  \"rotation\": [" << node.rotation.x << ", " << node.rotation.y << ", " << node.rotation.z << "],\n";
        oss << ind << "  \"scale\": [" << node.scale.x << ", " << node.scale.y << ", " << node.scale.z << "],\n";
        oss << ind << "  \"color\": [" << node.color.x << ", " << node.color.y << ", " << node.color.z << "],\n";
        oss << ind << "  \"visible\": " << (node.visible ? "true" : "false") << ",\n";
        oss << ind << "  \"children\": [";
        if (node.children.empty()) {
            oss << "]\n";
        } else {
            oss << "\n";
            for (size_t i = 0; i < node.children.size(); i++) {
                write_node(node.children[i], indent + 2);
                if (i < node.children.size() - 1) oss << ",";
                oss << "\n";
            }
            oss << ind << "  ]\n";
        }
        oss << ind << "}";
    };
    
    oss << "{\n";
    oss << "  \"prefab_name\": \"" << m_name << "\",\n";
    oss << "  \"version\": 1,\n";
    oss << "  \"root\": ";
    write_node(m_root_data, 1);
    oss << "\n}\n";
    
    return oss.str();
}

bool Prefab::FromJson(const std::string& json) {
    // Simple JSON parsing (manual, basic parser)
    // This is a minimal implementation - a real engine would use a JSON library
    
    auto find_string = [&json](const std::string& key, size_t start) -> std::string {
        std::string search = "\"" + key + "\": \"";
        size_t pos = json.find(search, start);
        if (pos == std::string::npos) return "";
        pos += search.length();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };
    
    auto find_array3 = [&json](const std::string& key, size_t start) -> vec3 {
        std::string search = "\"" + key + "\": [";
        size_t pos = json.find(search, start);
        if (pos == std::string::npos) return vec3{0, 0, 0};
        pos += search.length();
        size_t end = json.find("]", pos);
        if (end == std::string::npos) return vec3{0, 0, 0};
        std::string arr = json.substr(pos, end - pos);
        vec3 result{0, 0, 0};
        if (sscanf(arr.c_str(), "%f, %f, %f", &result.x, &result.y, &result.z) != 3) {
            // Try without spaces
            sscanf(arr.c_str(), "%f,%f,%f", &result.x, &result.y, &result.z);
        }
        return result;
    };
    
    auto find_bool = [&json](const std::string& key, size_t start) -> bool {
        std::string search = "\"" + key + "\": ";
        size_t pos = json.find(search, start);
        if (pos == std::string::npos) return true;
        pos += search.length();
        return json.substr(pos, 4) == "true";
    };
    
    // Parse prefab name
    m_name = find_string("prefab_name", 0);
    
    // Parse root node (simple version - single node, no children for now)
    size_t root_pos = json.find("\"root\":");
    if (root_pos == std::string::npos) return false;
    
    m_root_data.name = find_string("name", root_pos);
    m_root_data.type = find_string("type", root_pos);
    m_root_data.position = find_array3("position", root_pos);
    m_root_data.rotation = find_array3("rotation", root_pos);
    m_root_data.scale = find_array3("scale", root_pos);
    m_root_data.color = find_array3("color", root_pos);
    m_root_data.visible = find_bool("visible", root_pos);
    
    return true;
}

bool Prefab::SaveToFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to save prefab to: {}", path);
        return false;
    }
    
    file << ToJson();
    file.close();
    
    m_path = path;
    LOG_INFO("Saved prefab '{}' to: {}", m_name, path);
    return true;
}

bool Prefab::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to load prefab from: {}", path);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();
    
    if (!FromJson(json)) {
        LOG_ERROR("Failed to parse prefab JSON: {}", path);
        return false;
    }
    
    m_path = path;
    LOG_INFO("Loaded prefab '{}' from: {}", m_name, path);
    return true;
}

// ============================================================================
// PrefabManager
// ============================================================================

void PrefabManager::Initialize(Editor* editor, AssetManager* assets) {
    m_editor = editor;
    m_assets = assets;
    LOG_INFO("PrefabManager initialized");
}

std::shared_ptr<Prefab> PrefabManager::CreatePrefab(const std::string& name, const EditorNode& node) {
    auto prefab = std::make_shared<Prefab>(name);
    prefab->CaptureFromNode(node);
    m_prefabs[name] = prefab;
    LOG_INFO("Created prefab: {}", name);
    return prefab;
}

std::shared_ptr<Prefab> PrefabManager::LoadPrefab(const std::string& path) {
    auto prefab = std::make_shared<Prefab>();
    if (!prefab->LoadFromFile(path)) {
        return nullptr;
    }
    m_prefabs[prefab->GetName()] = prefab;
    return prefab;
}

bool PrefabManager::SavePrefab(std::shared_ptr<Prefab> prefab, const std::string& path) {
    if (!prefab) return false;
    return prefab->SaveToFile(path);
}

std::shared_ptr<Prefab> PrefabManager::GetPrefab(const std::string& name) {
    auto it = m_prefabs.find(name);
    if (it != m_prefabs.end()) {
        return it->second;
    }
    return nullptr;
}

u32 PrefabManager::InstantiatePrefab(std::shared_ptr<Prefab> prefab, EditorNode* parent) {
    if (!prefab || !m_editor) return 0;
    
    // Create nodes recursively
    const PrefabNodeData& root_data = prefab->GetRootData();
    
    // Use the editor to add the root node
    EditorNode* root_node = m_editor->AddNode(root_data.type, parent);
    if (!root_node) return 0;
    
    // Set properties
    root_node->name = root_data.name + " (Instance)";
    root_node->position = root_data.position;
    root_node->rotation = root_data.rotation;
    root_node->scale = root_data.scale;
    root_node->color = root_data.color;
    root_node->visible = root_data.visible;
    
    // Instantiate children recursively
    for (const auto& child_data : root_data.children) {
        InstantiateNodeRecursive(child_data, root_node);
    }
    
    LOG_INFO("Instantiated prefab: {}", prefab->GetName());
    return root_node->id;
}

void PrefabManager::InstantiateNodeRecursive(const PrefabNodeData& data, EditorNode* parent) {
    if (!m_editor || !parent) return;
    
    EditorNode* node = m_editor->AddNode(data.type, parent);
    if (!node) return;
    
    node->name = data.name;
    node->position = data.position;
    node->rotation = data.rotation;
    node->scale = data.scale;
    node->color = data.color;
    node->visible = data.visible;
    
    // Recurse into children
    for (const auto& child_data : data.children) {
        InstantiateNodeRecursive(child_data, node);
    }
}

void PrefabManager::UnloadPrefab(const std::string& name) {
    auto it = m_prefabs.find(name);
    if (it != m_prefabs.end()) {
        m_prefabs.erase(it);
        LOG_INFO("Unloaded prefab: {}", name);
    }
}

void PrefabManager::Clear() {
    m_prefabs.clear();
    LOG_INFO("Cleared all prefabs");
}

void PrefabManager::ScanPrefabDirectory() {
    // TODO: Scan directory for .prefab files and load them
    // This would use filesystem to iterate over files
    LOG_INFO("Scanning prefab directory: {}", m_prefab_directory);
}

} // namespace action
