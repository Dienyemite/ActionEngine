#pragma once

#include "core/types.h"
#include <functional>
#include <string>
#include <vector>

namespace action {

// Forward declare from editor.h
struct EditorNode;
class Editor;

using SceneTreeDeleteCallback = std::function<void(u32)>;

/*
 * SceneTreePanel - Hierarchical node browser (Godot-style)
 * 
 * Features:
 * - Tree view of scene nodes
 * - Multi-select with Ctrl+Click
 * - Drag & drop reordering (future)
 * - Right-click context menu
 * - Node visibility toggle
 */

class SceneTreePanel {
public:
    SceneTreePanel() = default;
    ~SceneTreePanel() = default;
    
    void Draw(EditorNode& root, u32& selected_id, std::vector<u32>& selected_ids);
    
    // Set callback for delete action
    void SetDeleteCallback(SceneTreeDeleteCallback callback) { m_delete_callback = callback; }
    
    bool visible = true;
    
private:
    void DrawNode(EditorNode& node, u32& selected_id, std::vector<u32>& selected_ids);
    void DrawContextMenu(EditorNode& node);

    // Find node by id in the tree; returns nullptr if not found
    EditorNode* FindNode(EditorNode& root, u32 id);
    // Find parent of node with given id; returns nullptr if root or not found
    EditorNode* FindParent(EditorNode& root, u32 child_id);
    // Duplicate a node (deep copy, assigns new sequential IDs)
    EditorNode DuplicateNode(const EditorNode& src);
    
    const char* GetNodeIcon(const std::string& type);
    bool IsSelected(u32 node_id, const std::vector<u32>& selected_ids);
    
    SceneTreeDeleteCallback m_delete_callback;
    u32 m_pending_delete_id    = 0;    // Node ID to delete (processed after ImGui frame)
    u32 m_pending_rename_id    = 0;    // Node ID to rename
    u32 m_pending_duplicate_id = 0;    // Node ID to duplicate
    u32 m_pending_reparent_src = 0;    // Node being reparented (drag-drop)
    u32 m_pending_reparent_dst = 0;    // New parent node ID
    u32 m_next_id_counter      = 1000; // Simple counter for new node IDs
    char m_rename_buf[128]     = {};   // Buffer for rename InputText
};

} // namespace action
