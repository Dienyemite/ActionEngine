#include "node.h"
#include "scene_tree.h"
#include "core/logging.h"
#include <algorithm>
#include <sstream>

namespace action {

NodeID Node::s_next_id = 1;

Node::Node() 
    : m_id(s_next_id++)
    , m_name("Node") 
{
}

Node::Node(const std::string& name)
    : m_id(s_next_id++)
    , m_name(name)
{
}

Node::~Node() {
    // Remove from groups
    if (m_tree) {
        for (const auto& group : m_groups) {
            m_tree->RemoveFromGroup(group, this);
        }
    }
    
    // Children are automatically cleaned up via shared_ptr
}

Node::Node(Node&& other) noexcept
    : m_id(other.m_id)
    , m_name(std::move(other.m_name))
    , m_parent(other.m_parent)
    , m_children(std::move(other.m_children))
    , m_tree(other.m_tree)
    , m_inside_tree(other.m_inside_tree)
    , m_process_mode(other.m_process_mode)
    , m_visible(other.m_visible)
    , m_editor_only(other.m_editor_only)
    , m_groups(std::move(other.m_groups))
    , m_signals(std::move(other.m_signals))
{
    other.m_id = INVALID_NODE_ID;
    other.m_parent = nullptr;
    other.m_tree = nullptr;
    other.m_inside_tree = false;
    
    // Update children's parent pointer
    for (auto& child : m_children) {
        child->m_parent = this;
    }
}

Node& Node::operator=(Node&& other) noexcept {
    if (this != &other) {
        m_id = other.m_id;
        m_name = std::move(other.m_name);
        m_parent = other.m_parent;
        m_children = std::move(other.m_children);
        m_tree = other.m_tree;
        m_inside_tree = other.m_inside_tree;
        m_process_mode = other.m_process_mode;
        m_visible = other.m_visible;
        m_editor_only = other.m_editor_only;
        m_groups = std::move(other.m_groups);
        m_signals = std::move(other.m_signals);
        
        other.m_id = INVALID_NODE_ID;
        other.m_parent = nullptr;
        other.m_tree = nullptr;
        other.m_inside_tree = false;
        
        for (auto& child : m_children) {
            child->m_parent = this;
        }
    }
    return *this;
}

// ===== Child Management =====

void Node::AddChild(std::shared_ptr<Node> child) {
    if (!child || child.get() == this) {
        LOG_WARN("Cannot add null or self as child");
        return;
    }
    
    if (child->m_parent) {
        child->m_parent->RemoveChild(child.get());
    }
    
    child->m_parent = this;
    m_children.push_back(child);
    
    // If we're in the tree, add child to tree too
    if (m_inside_tree && m_tree) {
        child->EnterTree(m_tree);
    }
}

void Node::RemoveChild(Node* child) {
    if (!child) return;
    
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const std::shared_ptr<Node>& n) { return n.get() == child; });
    
    if (it != m_children.end()) {
        if (child->m_inside_tree) {
            child->ExitTree();
        }
        child->m_parent = nullptr;
        m_children.erase(it);
    }
}

void Node::RemoveChild(size_t index) {
    if (index >= m_children.size()) return;
    
    auto& child = m_children[index];
    if (child->m_inside_tree) {
        child->ExitTree();
    }
    child->m_parent = nullptr;
    m_children.erase(m_children.begin() + index);
}

std::shared_ptr<Node> Node::GetChild(size_t index) const {
    if (index >= m_children.size()) return nullptr;
    return m_children[index];
}

std::shared_ptr<Node> Node::GetChildByName(const std::string& name) const {
    for (const auto& child : m_children) {
        if (child->m_name == name) {
            return child;
        }
    }
    return nullptr;
}

int Node::GetChildIndex(Node* child) const {
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i].get() == child) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Node::MoveChild(size_t from_index, size_t to_index) {
    if (from_index >= m_children.size() || to_index >= m_children.size()) return;
    if (from_index == to_index) return;
    
    auto child = m_children[from_index];
    m_children.erase(m_children.begin() + from_index);
    m_children.insert(m_children.begin() + to_index, child);
}

// ===== Tree Queries =====

Node* Node::GetRoot() const {
    const Node* current = this;
    while (current->m_parent) {
        current = current->m_parent;
    }
    return const_cast<Node*>(current);
}

std::string Node::GetPath() const {
    std::vector<std::string> parts;
    const Node* current = this;
    
    while (current) {
        parts.push_back(current->m_name);
        current = current->m_parent;
    }
    
    std::stringstream ss;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        ss << "/" << *it;
    }
    
    return ss.str();
}

Node* Node::GetNodeByPath(const std::string& path) const {
    if (path.empty()) return nullptr;
    
    // Absolute path (starts from root)
    if (path[0] == '/') {
        Node* root = GetRoot();
        if (path == "/") return root;
        
        std::string rel_path = path.substr(1);
        // Skip root name if it matches
        size_t first_slash = rel_path.find('/');
        if (first_slash != std::string::npos) {
            std::string first_part = rel_path.substr(0, first_slash);
            if (first_part == root->m_name) {
                rel_path = rel_path.substr(first_slash + 1);
            }
        } else if (rel_path == root->m_name) {
            return root;
        }
        
        return root->GetNodeByPath(rel_path);
    }
    
    // Relative path
    std::string remaining = path;
    const Node* current = this;
    
    while (!remaining.empty() && current) {
        size_t slash = remaining.find('/');
        std::string part = (slash == std::string::npos) 
            ? remaining : remaining.substr(0, slash);
        
        if (part == "..") {
            current = current->m_parent;
        } else if (part == "." || part.empty()) {
            // Stay at current
        } else {
            // Find child with this name
            auto child = current->GetChildByName(part);
            current = child.get();
        }
        
        if (slash == std::string::npos) break;
        remaining = remaining.substr(slash + 1);
    }
    
    return const_cast<Node*>(current);
}

bool Node::IsAncestorOf(Node* node) const {
    if (!node) return false;
    
    Node* parent = node->m_parent;
    while (parent) {
        if (parent == this) return true;
        parent = parent->m_parent;
    }
    return false;
}

bool Node::IsDescendantOf(Node* node) const {
    if (!node) return false;
    return node->IsAncestorOf(const_cast<Node*>(this));
}

void Node::Reparent(Node* new_parent) {
    if (new_parent == m_parent) return;
    if (new_parent == this) {
        LOG_WARN("Cannot reparent node to itself");
        return;
    }
    if (new_parent && IsAncestorOf(new_parent)) {
        LOG_WARN("Cannot reparent node to its descendant");
        return;
    }
    
    // Get shared_ptr to self from parent
    std::shared_ptr<Node> self_ptr;
    if (m_parent) {
        for (const auto& sibling : m_parent->m_children) {
            if (sibling.get() == this) {
                self_ptr = sibling;
                break;
            }
        }
        m_parent->RemoveChild(this);
    }
    
    if (new_parent && self_ptr) {
        new_parent->AddChild(self_ptr);
    }
}

// ===== Lifecycle =====

void Node::EnterTree(SceneTree* tree) {
    if (m_inside_tree) return;

    m_tree = tree;
    m_inside_tree = true;

    // Register this node in the tree's node registry for ID-based lookup.
    // Without this, nodes added via AddChild() after SetRoot() would never
    // appear in the registry and GetNodeByID() would fail for them.
    tree->RegisterNode(this);

    // Register with groups
    for (const auto& group : m_groups) {
        tree->AddToGroup(group, this);
    }

    // Call _Ready for this node
    _Ready();

    // Propagate to children
    for (auto& child : m_children) {
        child->EnterTree(tree);
    }
}

void Node::ExitTree() {
    if (!m_inside_tree) return;

    // Exit children first (reverse order)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        (*it)->ExitTree();
    }

    // Call _ExitTree
    _ExitTree();

    if (m_tree) {
        // Unregister from groups
        for (const auto& group : m_groups) {
            m_tree->RemoveFromGroup(group, this);
        }

        // Unregister from node registry (symmetric with RegisterNode in EnterTree)
        m_tree->UnregisterNode(this);
    }

    m_inside_tree = false;
    m_tree = nullptr;
}

bool Node::CanProcess() const {
    switch (m_process_mode) {
        case ProcessMode::Disabled:
            return false;
        case ProcessMode::Always:
            return true;
        case ProcessMode::Inherit:
            return m_parent ? m_parent->CanProcess() : true;
        case ProcessMode::Pausable:
            return m_tree ? !m_tree->IsPaused() : true;
        case ProcessMode::WhenPaused:
            return m_tree ? m_tree->IsPaused() : false;
    }
    return true;
}

void Node::PropagateProcess(float delta) {
    if (!CanProcess()) return;
    
    _Process(delta);
    
    for (auto& child : m_children) {
        child->PropagateProcess(delta);
    }
}

void Node::PropagatePhysicsProcess(float delta) {
    if (!CanProcess()) return;
    
    _PhysicsProcess(delta);
    
    for (auto& child : m_children) {
        child->PropagatePhysicsProcess(delta);
    }
}

// ===== Visibility =====

bool Node::IsVisibleInTree() const {
    if (!m_visible) return false;
    if (m_parent) return m_parent->IsVisibleInTree();
    return true;
}

// ===== Groups =====

void Node::AddToGroup(const std::string& group) {
    if (IsInGroup(group)) return;
    
    m_groups.push_back(group);
    
    if (m_tree) {
        m_tree->AddToGroup(group, this);
    }
}

void Node::RemoveFromGroup(const std::string& group) {
    auto it = std::find(m_groups.begin(), m_groups.end(), group);
    if (it != m_groups.end()) {
        m_groups.erase(it);
        if (m_tree) {
            m_tree->RemoveFromGroup(group, this);
        }
    }
}

bool Node::IsInGroup(const std::string& group) const {
    return std::find(m_groups.begin(), m_groups.end(), group) != m_groups.end();
}

// ===== Signals =====

void Node::EmitSignal(const std::string& signal_name) {
    auto it = m_signals.find(signal_name);
    if (it == m_signals.end()) return;
    
    for (auto& conn : it->second) {
        if (m_tree) {
            Node* target = m_tree->GetNodeByID(conn.target_node);
            if (target) {
                target->_OnSignal(conn.method_name, this);
            }
        }
    }
    
    // Remove oneshot connections
    it->second.erase(
        std::remove_if(it->second.begin(), it->second.end(),
            [](const SignalConnection& c) { return c.oneshot; }),
        it->second.end()
    );
}

void Node::Connect(const std::string& signal, Node* target, const std::string& method) {
    if (!target) return;
    
    SignalConnection conn;
    conn.target_node = target->GetID();
    conn.method_name = method;
    
    m_signals[signal].push_back(conn);
}

void Node::Disconnect(const std::string& signal, Node* target, const std::string& method) {
    auto it = m_signals.find(signal);
    if (it == m_signals.end()) return;
    
    NodeID target_id = target ? target->GetID() : INVALID_NODE_ID;
    
    it->second.erase(
        std::remove_if(it->second.begin(), it->second.end(),
            [target_id, &method](const SignalConnection& c) {
                return c.target_node == target_id && c.method_name == method;
            }),
        it->second.end()
    );
}

// ===== Serialization =====

void Node::Serialize(Serializer& s) const {
    // Base implementation - override in derived classes
}

void Node::Deserialize(Deserializer& d) {
    // Base implementation - override in derived classes
}

} // namespace action
