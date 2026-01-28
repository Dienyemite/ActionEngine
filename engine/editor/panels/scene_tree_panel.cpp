#include "scene_tree_panel.h"
#include "editor/editor.h"
#include <imgui/imgui.h>
#include <algorithm>

namespace action {

void SceneTreePanel::Draw(EditorNode& root, u32& selected_id, std::vector<u32>& selected_ids) {
    if (!visible) return;
    
    if (ImGui::Begin("Scene", &visible)) {
        // Search/filter bar (future)
        static char search_buffer[128] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##search", "Search nodes...", search_buffer, sizeof(search_buffer));
        
        // Multi-select hint
        ImGui::TextDisabled("Ctrl+Click for multi-select");
        
        ImGui::Separator();
        
        // Draw the scene tree
        DrawNode(root, selected_id, selected_ids);
    }
    ImGui::End();
}

void SceneTreePanel::DrawNode(EditorNode& node, u32& selected_id, std::vector<u32>& selected_ids) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | 
                                ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                ImGuiTreeNodeFlags_SpanAvailWidth;
    
    // Leaf nodes don't have the arrow
    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    
    // Multi-select highlight: check if in selected_ids
    bool is_selected = IsSelected(node.id, selected_ids);
    if (is_selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Default root to open
    if (node.expanded) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    
    // Node icon based on type
    const char* icon = GetNodeIcon(node.type);
    
    // Build display name with icon
    char display_name[256];
    snprintf(display_name, sizeof(display_name), "%s %s", icon, node.name.c_str());
    
    // Push ID to avoid conflicts
    ImGui::PushID(node.id);
    
    // Draw visibility toggle
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    bool vis = node.visible;
    if (ImGui::Checkbox("##vis", &vis)) {
        node.visible = vis;
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();
    
    // Draw tree node
    bool opened = ImGui::TreeNodeEx(display_name, flags);
    
    // Handle selection with Ctrl modifier for multi-select
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        ImGuiIO& io = ImGui::GetIO();
        
        if (io.KeyCtrl) {
            // Ctrl+Click: Toggle selection in multi-select
            auto it = std::find(selected_ids.begin(), selected_ids.end(), node.id);
            if (it != selected_ids.end()) {
                // Already selected - remove from selection
                selected_ids.erase(it);
                // Update primary selection to last item or 0
                if (!selected_ids.empty()) {
                    selected_id = selected_ids.back();
                } else {
                    selected_id = 0;
                }
            } else {
                // Not selected - add to selection
                selected_ids.push_back(node.id);
                selected_id = node.id;  // Make it the primary selection
            }
        } else {
            // Regular click: Single select (clears multi-select)
            selected_ids.clear();
            selected_ids.push_back(node.id);
            selected_id = node.id;
        }
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        DrawContextMenu(node);
        ImGui::EndPopup();
    }
    
    // Drag & drop source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("SCENE_NODE", &node.id, sizeof(u32));
        ImGui::Text("%s %s", icon, node.name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Drag & drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            u32 dropped_id = *(const u32*)payload->Data;
            // TODO: Implement reparenting
            (void)dropped_id;
        }
        ImGui::EndDragDropTarget();
    }
    
    // Draw children if opened
    if (opened) {
        for (auto& child : node.children) {
            DrawNode(child, selected_id, selected_ids);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

void SceneTreePanel::DrawContextMenu(EditorNode& node) {
    if (ImGui::MenuItem("Add Child Node")) {
        // TODO: Open add node dialog
    }
    if (ImGui::MenuItem("Instance Scene")) {
        // TODO: Open scene browser
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        // TODO: Duplicate node
    }
    if (ImGui::MenuItem("Rename", "F2")) {
        // TODO: Rename node
    }
    if (ImGui::MenuItem("Delete", "Del")) {
        // TODO: Delete node
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
    if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
    
    ImGui::Separator();
    
    // Quick access to common operations
    if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("Node3D")) {}
        if (ImGui::MenuItem("MeshInstance3D")) {}
        if (ImGui::MenuItem("Camera3D")) {}
        if (ImGui::MenuItem("DirectionalLight")) {}
        if (ImGui::MenuItem("PointLight")) {}
        ImGui::EndMenu();
    }
}

const char* SceneTreePanel::GetNodeIcon(const std::string& type) {
    // Return simple text icons (in future could use proper icons)
    if (type == "Node" || type == "Node3D") return "[N]";
    if (type == "Camera3D") return "[C]";
    if (type == "MeshInstance3D") return "[M]";
    if (type == "DirectionalLight") return "[D]";
    if (type == "PointLight") return "[P]";
    if (type == "SpotLight") return "[S]";
    if (type == "RigidBody") return "[R]";
    if (type == "StaticBody") return "[B]";
    if (type == "Area3D") return "[A]";
    if (type == "CollisionShape") return "[>]";
    return "[?]";
}

bool SceneTreePanel::IsSelected(u32 node_id, const std::vector<u32>& selected_ids) {
    return std::find(selected_ids.begin(), selected_ids.end(), node_id) != selected_ids.end();
}

} // namespace action
