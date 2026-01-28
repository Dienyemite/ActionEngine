#pragma once

#include "core/types.h"
#include <string>
#include <vector>

namespace action {

// Forward declare from editor.h
struct EditorNode;
class Editor;

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
    
    bool visible = true;
    
private:
    void DrawNode(EditorNode& node, u32& selected_id, std::vector<u32>& selected_ids);
    void DrawContextMenu(EditorNode& node);
    
    const char* GetNodeIcon(const std::string& type);
    bool IsSelected(u32 node_id, const std::vector<u32>& selected_ids);
};

} // namespace action
