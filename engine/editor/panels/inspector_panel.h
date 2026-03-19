#pragma once

#include "core/types.h"
#include "scripting/script_system.h"
#include <string>
#include <functional>

namespace action {

// Forward declare from editor.h  
struct EditorNode;
class ECS;

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

    // Provide ECS + ScriptSystem so the Scripts section is live.
    void SetScriptSystem(ScriptSystem* scripts, ECS* ecs) { m_scripts = scripts; m_ecs = ecs; }
    
    // Set callback for delete action
    void SetDeleteCallback(DeleteCallback callback) { m_delete_callback = callback; }

    // Set callback invoked when the user requests a new C++ script file.
    using CreateScriptCallback = std::function<void(const std::string&)>;
    void SetCreateScriptCallback(CreateScriptCallback cb) { m_create_script_cb = std::move(cb); }

    bool visible = true;
    
private:
    void DrawNodeHeader(EditorNode& node);
    void DrawNodeInfo(EditorNode& node);
    void DrawTransform(EditorNode& node);
    void DrawNodeProperties(EditorNode& node);
    bool SectionVisible(const char* section_name) const;
    
    // Property helpers
    bool DrawVec3(const char* label, vec3& value, float reset_value = 0.0f);
    bool DrawFloat(const char* label, float& value, float min = 0.0f, float max = 0.0f);
    bool DrawBool(const char* label, bool& value);
    bool DrawString(const char* label, std::string& value);
    bool DrawColor(const char* label, vec3& color);
    
    DeleteCallback m_delete_callback;
    u32 m_pending_delete_id = 0;  // Node ID to delete (processed after ImGui frame)

    // Script system references (set by Editor)
    ScriptSystem* m_scripts = nullptr;
    ECS*          m_ecs     = nullptr;
    
    // Mesh browser modal state
    bool   m_show_mesh_browser = false;
    char   m_mesh_filter[128]  = {};
    
    // Script dialog modal state
    bool   m_show_script_dialog  = false;
    char   m_script_class[128]   = {};
    char   m_script_filter[128]  = {};

    // Create new script dialog state
    bool                 m_show_create_script_dialog = false;
    char                 m_new_script_name[64]       = {};
    CreateScriptCallback m_create_script_cb;

    // Search filter
    char m_search_buf[128] = {};
};

} // namespace action
