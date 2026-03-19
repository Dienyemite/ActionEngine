#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace action {

struct EditorNode;

/*
 * GroupsPanel — Godot-style node groups manager.
 *
 * Allows tagging EditorNodes into named groups (like Godot's groups system).
 * Useful for batch operations: "add all enemies to group 'enemies'".
 */
class GroupsPanel {
public:
    GroupsPanel() = default;
    ~GroupsPanel() = default;

    // Draw the panel. Needs access to the scene root to enumerate nodes.
    void Draw(EditorNode& scene_root, u32& selected_node_id);

    // Group management
    void AddNodeToGroup(u32 node_id, const std::string& group_name);
    void RemoveNodeFromGroup(u32 node_id, const std::string& group_name);
    bool IsNodeInGroup(u32 node_id, const std::string& group_name) const;
    std::vector<std::string> GetNodeGroups(u32 node_id) const;
    std::vector<u32> GetGroupMembers(const std::string& group_name) const;
    const std::unordered_map<std::string, std::unordered_set<u32>>& GetAllGroups() const { return m_groups; }

    // Set callback invoked when selection changes from the groups panel
    using SelectionCallback = std::function<void(u32)>;
    void SetSelectionCallback(SelectionCallback cb) { m_selection_cb = std::move(cb); }

    bool visible = false;

private:
    void DrawGroupList(EditorNode& scene_root);
    void DrawNodeGroupEditor(EditorNode& scene_root, u32 selected_id);
    void CollectAllNodes(EditorNode& root, std::vector<EditorNode*>& out);
    EditorNode* FindNode(EditorNode& root, u32 id);

    // group name -> set of node IDs
    std::unordered_map<std::string, std::unordered_set<u32>> m_groups;

    char m_new_group_name[64] = {};
    std::string m_selected_group;

    SelectionCallback m_selection_cb;
};

} // namespace action
