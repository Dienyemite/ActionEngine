#pragma once

#include "node.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <functional>

namespace action {

/*
 * SceneTree - Manages the node hierarchy (Godot-style)
 * 
 * Features:
 * - Root node management
 * - Group system for batch operations
 * - Pause system
 * - Node lookup by ID or path
 * - Deferred calls (call_deferred equivalent)
 * - Frame callbacks
 */

// Deferred call entry
struct DeferredCall {
    std::function<void()> callable;
    float delay = 0;  // 0 = next frame
};

class SceneTree {
public:
    SceneTree() = default;
    ~SceneTree() = default;
    
    bool Initialize();
    void Shutdown();
    
    // ===== Root Node =====
    void SetRoot(std::shared_ptr<Node> root);
    Node* GetRoot() const { return m_root.get(); }
    std::shared_ptr<Node> GetRootShared() const { return m_root; }
    
    // ===== Frame Processing =====
    void Process(float delta);
    void PhysicsProcess(float delta);
    
    // ===== Pause System =====
    bool IsPaused() const { return m_paused; }
    void SetPaused(bool paused) { m_paused = paused; }
    void TogglePause() { m_paused = !m_paused; }
    
    // ===== Node Lookup =====
    Node* GetNodeByID(NodeID id) const;
    Node* GetNodeByPath(const std::string& path) const;
    
    template<typename T>
    T* GetNodeAs(const std::string& path) const {
        Node* node = GetNodeByPath(path);
        return dynamic_cast<T*>(node);
    }
    
    // ===== Groups =====
    void AddToGroup(const std::string& group, Node* node);
    void RemoveFromGroup(const std::string& group, Node* node);
    std::vector<Node*> GetNodesInGroup(const std::string& group) const;
    void CallGroup(const std::string& group, const std::string& method);
    bool HasGroup(const std::string& group) const;
    
    // ===== Deferred Calls =====
    void CallDeferred(std::function<void()> callable);
    void CallDelayed(float delay, std::function<void()> callable);
    
    // ===== Scene Management =====
    void ChangeScene(std::shared_ptr<Node> new_root);
    void ReloadCurrentScene();
    
    // Get current scene file path (if loaded from file)
    const std::string& GetCurrentScenePath() const { return m_current_scene_path; }
    void SetCurrentScenePath(const std::string& path) { m_current_scene_path = path; }
    
    // ===== Callbacks =====
    using FrameCallback = std::function<void(float)>;
    void SetProcessCallback(FrameCallback callback) { m_process_callback = callback; }
    void SetPhysicsCallback(FrameCallback callback) { m_physics_callback = callback; }
    
    // ===== Tree Iteration =====
    template<typename Func>
    void ForEachNode(Func&& func);
    
    template<typename T, typename Func>
    void ForEachNodeOfType(Func&& func);
    
    // ===== Statistics =====
    size_t GetNodeCount() const { return m_node_registry.size(); }
    
    // Node registration (called internally)
    void RegisterNode(Node* node);
    void UnregisterNode(Node* node);
    
private:
    void ProcessDeferredCalls(float delta);
    void ForEachNodeRecursive(Node* node, const std::function<void(Node*)>& func);
    
    std::shared_ptr<Node> m_root;
    std::string m_current_scene_path;
    
    // Pause state
    bool m_paused = false;
    
    // Node registry for fast lookup by ID
    std::unordered_map<NodeID, Node*> m_node_registry;
    
    // Group system
    std::unordered_map<std::string, std::unordered_set<Node*>> m_groups;
    
    // Deferred calls
    std::queue<DeferredCall> m_deferred_calls;
    std::vector<DeferredCall> m_delayed_calls;
    
    // Callbacks
    FrameCallback m_process_callback;
    FrameCallback m_physics_callback;
};

// Template implementations
template<typename Func>
void SceneTree::ForEachNode(Func&& func) {
    if (m_root) {
        ForEachNodeRecursive(m_root.get(), std::forward<Func>(func));
    }
}

template<typename T, typename Func>
void SceneTree::ForEachNodeOfType(Func&& func) {
    ForEachNode([&func](Node* node) {
        if (T* typed = dynamic_cast<T*>(node)) {
            func(typed);
        }
    });
}

} // namespace action
