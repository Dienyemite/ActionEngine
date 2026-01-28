#include "scene_tree.h"
#include "core/logging.h"
#include <algorithm>

namespace action {

bool SceneTree::Initialize() {
    LOG_INFO("SceneTree initialized");
    return true;
}

void SceneTree::Shutdown() {
    // Exit tree for all nodes
    if (m_root && m_root->IsInsideTree()) {
        m_root->ExitTree();
    }
    
    m_root.reset();
    m_node_registry.clear();
    m_groups.clear();
    
    // Clear deferred calls
    while (!m_deferred_calls.empty()) {
        m_deferred_calls.pop();
    }
    m_delayed_calls.clear();
    
    LOG_INFO("SceneTree shutdown");
}

void SceneTree::SetRoot(std::shared_ptr<Node> root) {
    // Exit old root
    if (m_root && m_root->IsInsideTree()) {
        m_root->ExitTree();
    }
    
    m_root = root;
    
    // Enter new root
    if (m_root) {
        RegisterNode(m_root.get());
        m_root->EnterTree(this);
    }
}

void SceneTree::Process(float delta) {
    // Process deferred calls first
    ProcessDeferredCalls(delta);
    
    // Custom callback
    if (m_process_callback) {
        m_process_callback(delta);
    }
    
    // Process all nodes (respects pause state via CanProcess)
    if (m_root) {
        m_root->PropagateProcess(delta);
    }
}

void SceneTree::PhysicsProcess(float delta) {
    // Custom callback
    if (m_physics_callback) {
        m_physics_callback(delta);
    }
    
    // Physics process all nodes
    if (m_root) {
        m_root->PropagatePhysicsProcess(delta);
    }
}

void SceneTree::ProcessDeferredCalls(float delta) {
    // Process immediate deferred calls
    size_t count = m_deferred_calls.size();
    for (size_t i = 0; i < count; ++i) {
        auto call = std::move(m_deferred_calls.front());
        m_deferred_calls.pop();
        call.callable();
    }
    
    // Process delayed calls
    for (auto it = m_delayed_calls.begin(); it != m_delayed_calls.end();) {
        it->delay -= delta;
        if (it->delay <= 0) {
            it->callable();
            it = m_delayed_calls.erase(it);
        } else {
            ++it;
        }
    }
}

Node* SceneTree::GetNodeByID(NodeID id) const {
    auto it = m_node_registry.find(id);
    return it != m_node_registry.end() ? it->second : nullptr;
}

Node* SceneTree::GetNodeByPath(const std::string& path) const {
    if (!m_root) return nullptr;
    return m_root->GetNodeByPath(path);
}

void SceneTree::RegisterNode(Node* node) {
    if (!node) return;
    m_node_registry[node->GetID()] = node;
    
    // Register children recursively
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        RegisterNode(node->GetChild(i).get());
    }
}

void SceneTree::UnregisterNode(Node* node) {
    if (!node) return;
    m_node_registry.erase(node->GetID());
    
    // Unregister children recursively
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        UnregisterNode(node->GetChild(i).get());
    }
}

// ===== Groups =====

void SceneTree::AddToGroup(const std::string& group, Node* node) {
    if (!node) return;
    m_groups[group].insert(node);
}

void SceneTree::RemoveFromGroup(const std::string& group, Node* node) {
    auto it = m_groups.find(group);
    if (it != m_groups.end()) {
        it->second.erase(node);
        if (it->second.empty()) {
            m_groups.erase(it);
        }
    }
}

std::vector<Node*> SceneTree::GetNodesInGroup(const std::string& group) const {
    auto it = m_groups.find(group);
    if (it != m_groups.end()) {
        return std::vector<Node*>(it->second.begin(), it->second.end());
    }
    return {};
}

void SceneTree::CallGroup(const std::string& group, const std::string& method) {
    auto nodes = GetNodesInGroup(group);
    for (Node* node : nodes) {
        node->_OnSignal(method, nullptr);
    }
}

bool SceneTree::HasGroup(const std::string& group) const {
    return m_groups.find(group) != m_groups.end();
}

// ===== Deferred Calls =====

void SceneTree::CallDeferred(std::function<void()> callable) {
    DeferredCall call;
    call.callable = std::move(callable);
    call.delay = 0;
    m_deferred_calls.push(std::move(call));
}

void SceneTree::CallDelayed(float delay, std::function<void()> callable) {
    DeferredCall call;
    call.callable = std::move(callable);
    call.delay = delay;
    m_delayed_calls.push_back(std::move(call));
}

// ===== Scene Management =====

void SceneTree::ChangeScene(std::shared_ptr<Node> new_root) {
    // Defer the actual scene change to avoid issues during processing
    CallDeferred([this, new_root]() {
        SetRoot(new_root);
    });
}

void SceneTree::ReloadCurrentScene() {
    if (m_current_scene_path.empty()) {
        LOG_WARN("No current scene path set, cannot reload");
        return;
    }
    
    // Scene loading will be implemented with serialization system
    LOG_INFO("Scene reload requested: %s", m_current_scene_path.c_str());
}

void SceneTree::ForEachNodeRecursive(Node* node, const std::function<void(Node*)>& func) {
    if (!node) return;
    
    func(node);
    
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        ForEachNodeRecursive(node->GetChild(i).get(), func);
    }
}

} // namespace action
