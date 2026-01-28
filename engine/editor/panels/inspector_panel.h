#pragma once

#include "core/types.h"
#include <string>
#include <functional>

namespace action {

// Forward declare from editor.h  
struct EditorNode;

/*
 * InspectorPanel - Property editor (Godot-style)
 * 
 * Features:
 * - Auto-generated property editors based on node type
 * - Transform editing (position, rotation, scale)
 * - Material properties
 * - Component/script properties
 * - Delete node button
 */

// Callback type for deletion
using DeleteCallback = std::function<void(u32)>;

class InspectorPanel {
public:
    InspectorPanel() = default;
    ~InspectorPanel() = default;
    
    void Draw(EditorNode* selected_node);
    
    // Set callback for delete action
    void SetDeleteCallback(DeleteCallback callback) { m_delete_callback = callback; }
    
    bool visible = true;
    
private:
    void DrawNodeHeader(EditorNode& node);
    void DrawTransform(EditorNode& node);
    void DrawNodeProperties(EditorNode& node);
    
    // Property helpers
    bool DrawVec3(const char* label, vec3& value, float reset_value = 0.0f);
    bool DrawFloat(const char* label, float& value, float min = 0.0f, float max = 0.0f);
    bool DrawBool(const char* label, bool& value);
    bool DrawString(const char* label, std::string& value);
    bool DrawColor(const char* label, vec3& color);
    
    DeleteCallback m_delete_callback;
    u32 m_pending_delete_id = 0;  // Node ID to delete (processed after ImGui frame)
};

} // namespace action
