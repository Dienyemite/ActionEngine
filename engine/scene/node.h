#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <typeindex>
#include <unordered_map>

namespace action {

// Forward declarations
class Node;
class SceneTree;

// Node unique identifier
using NodeID = u64;
constexpr NodeID INVALID_NODE_ID = 0;

/*
 * Node - Base class for all scene objects (Godot-style)
 * 
 * Features:
 * - Tree hierarchy (parent/children)
 * - Virtual lifecycle methods (_ready, _process, _physics_process)
 * - Signal/slot communication
 * - Group membership
 * - Pause mode support
 */

// Signal connection
struct SignalConnection {
    NodeID target_node;
    std::string method_name;
    bool oneshot = false;
    bool deferred = false;
};

// Pause mode for nodes
enum class PauseMode : u8 {
    Inherit = 0,    // Use parent's pause mode
    Stop,           // Pause when tree is paused
    Process,        // Continue processing when paused
};

// Process mode
enum class ProcessMode : u8 {
    Inherit = 0,
    Pausable,       // Pauses when tree pauses
    WhenPaused,     // Only processes when paused
    Always,         // Always processes
    Disabled,       // Never processes
};

class Node : public std::enable_shared_from_this<Node> {
public:
    Node();
    explicit Node(const std::string& name);
    virtual ~Node();
    
    // Prevent copying
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    
    // Move semantics
    Node(Node&& other) noexcept;
    Node& operator=(Node&& other) noexcept;
    
    // ===== Identification =====
    NodeID GetID() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }
    
    // Type name for serialization and editor
    virtual std::string GetTypeName() const { return "Node"; }
    static std::string GetStaticTypeName() { return "Node"; }
    
    // ===== Tree Hierarchy =====
    Node* GetParent() const { return m_parent; }
    const std::vector<std::shared_ptr<Node>>& GetChildren() const { return m_children; }
    size_t GetChildCount() const { return m_children.size(); }
    
    // Child management
    void AddChild(std::shared_ptr<Node> child);
    void RemoveChild(Node* child);
    void RemoveChild(size_t index);
    std::shared_ptr<Node> GetChild(size_t index) const;
    std::shared_ptr<Node> GetChildByName(const std::string& name) const;
    int GetChildIndex(Node* child) const;
    void MoveChild(size_t from_index, size_t to_index);
    
    // Tree queries
    Node* GetRoot() const;
    std::string GetPath() const;  // e.g., "/Root/Player/Camera"
    Node* GetNodeByPath(const std::string& path) const;
    template<typename T> T* GetNodeAs(const std::string& path) const;
    bool IsAncestorOf(Node* node) const;
    bool IsDescendantOf(Node* node) const;
    
    // Reparenting
    void Reparent(Node* new_parent);
    
    // ===== Lifecycle =====
    bool IsInsideTree() const { return m_inside_tree; }
    SceneTree* GetTree() const { return m_tree; }
    
    // Called when entering tree (implement in derived classes)
    virtual void _Ready() {}
    
    // Called every frame (implement in derived classes)
    virtual void _Process(float delta) {}
    
    // Called at fixed physics rate (implement in derived classes)
    virtual void _PhysicsProcess(float delta) {}
    
    // Called when exiting tree
    virtual void _ExitTree() {}
    
    // ===== Process Control =====
    ProcessMode GetProcessMode() const { return m_process_mode; }
    void SetProcessMode(ProcessMode mode) { m_process_mode = mode; }
    
    bool CanProcess() const;
    
    // ===== Visibility =====
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisibleInTree() const;
    
    // ===== Groups =====
    void AddToGroup(const std::string& group);
    void RemoveFromGroup(const std::string& group);
    bool IsInGroup(const std::string& group) const;
    const std::vector<std::string>& GetGroups() const { return m_groups; }
    
    // ===== Signals (simplified) =====
    void EmitSignal(const std::string& signal_name);
    void Connect(const std::string& signal, Node* target, const std::string& method);
    void Disconnect(const std::string& signal, Node* target, const std::string& method);
    
    // ===== Transform (basic, override in Node2D/Node3D) =====
    virtual vec3 GetPosition() const { return vec3{0, 0, 0}; }
    virtual void SetPosition(const vec3& pos) {}
    
    // ===== Serialization (override in derived) =====
    virtual void Serialize(class Serializer& s) const;
    virtual void Deserialize(class Deserializer& d);
    
    // ===== Editor Support =====
    bool IsEditorOnly() const { return m_editor_only; }
    void SetEditorOnly(bool editor_only) { m_editor_only = editor_only; }
    
protected:
    // Called by SceneTree
    friend class SceneTree;
    void EnterTree(SceneTree* tree);
    void ExitTree();
    void PropagateProcess(float delta);
    void PropagatePhysicsProcess(float delta);
    
    // Signal method binding (override to handle signals)
    virtual void _OnSignal(const std::string& signal, Node* from) {}
    
private:
    static NodeID s_next_id;
    
    NodeID m_id = INVALID_NODE_ID;
    std::string m_name;
    
    // Hierarchy
    Node* m_parent = nullptr;
    std::vector<std::shared_ptr<Node>> m_children;
    
    // Tree state
    SceneTree* m_tree = nullptr;
    bool m_inside_tree = false;
    
    // Properties
    ProcessMode m_process_mode = ProcessMode::Inherit;
    bool m_visible = true;
    bool m_editor_only = false;
    
    // Groups
    std::vector<std::string> m_groups;
    
    // Signals
    std::unordered_map<std::string, std::vector<SignalConnection>> m_signals;
};

// Template implementations
template<typename T>
T* Node::GetNodeAs(const std::string& path) const {
    Node* node = GetNodeByPath(path);
    return dynamic_cast<T*>(node);
}

} // namespace action
