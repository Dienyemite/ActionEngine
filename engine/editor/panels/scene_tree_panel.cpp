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
    
    // Process pending delete (outside of ImGui window context)
    if (m_pending_delete_id != 0 && m_delete_callback) {
        m_delete_callback(m_pending_delete_id);
        m_pending_delete_id = 0;
    }

    // Process pending rename: open a popup once per frame
    if (m_pending_rename_id != 0) {
        ImGui::OpenPopup("##RenamePopup");
    }
    if (ImGui::BeginPopup("##RenamePopup")) {
        ImGui::Text("Rename node:");
        if (m_pending_rename_id != 0)
            ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", m_rename_buf, sizeof(m_rename_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            EditorNode* target = FindNode(root, m_pending_rename_id);
            if (target && m_rename_buf[0] != '\0')
                target->name = m_rename_buf;
            m_pending_rename_id = 0;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel")) {
            m_pending_rename_id = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Process pending duplicate
    if (m_pending_duplicate_id != 0) {
        EditorNode* parent = FindParent(root, m_pending_duplicate_id);
        EditorNode* src    = FindNode(root, m_pending_duplicate_id);
        if (src && parent) {
            EditorNode copy = DuplicateNode(*src);
            parent->children.push_back(std::move(copy));
        }
        m_pending_duplicate_id = 0;
    }

    // Process pending reparent (drag-drop)
    if (m_pending_reparent_src != 0 && m_pending_reparent_dst != 0) {
        EditorNode* src_node    = FindNode(root, m_pending_reparent_src);
        EditorNode* old_parent  = FindParent(root, m_pending_reparent_src);
        EditorNode* new_parent  = FindNode(root, m_pending_reparent_dst);
        // Guard: don't reparent onto a descendant of src (would create cycle)
        if (src_node && old_parent && new_parent && src_node != new_parent
            && FindNode(*src_node, m_pending_reparent_dst) == nullptr) {
            // Extract from old parent
            auto& siblings = old_parent->children;
            auto it = std::find_if(siblings.begin(), siblings.end(),
                [&](const EditorNode& n){ return n.id == m_pending_reparent_src; });
            if (it != siblings.end()) {
                EditorNode moved = std::move(*it);
                siblings.erase(it);
                new_parent->children.push_back(std::move(moved));
                new_parent->expanded = true;
            }
        }
        m_pending_reparent_src = 0;
        m_pending_reparent_dst = 0;
    }
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
            if (dropped_id != node.id) {
                m_pending_reparent_src = dropped_id;
                m_pending_reparent_dst = node.id;
            }
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
        EditorNode child;
        child.id   = m_next_id_counter++;
        child.name = "Node" + std::to_string(child.id);
        child.type = "Node3D";
        node.children.push_back(std::move(child));
        node.expanded = true;
    }
    if (ImGui::MenuItem("Instance Scene")) {
        // TODO: Open scene browser
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        m_pending_duplicate_id = node.id;
    }
    if (ImGui::MenuItem("Rename", "F2")) {
        m_pending_rename_id = node.id;
        snprintf(m_rename_buf, sizeof(m_rename_buf), "%s", node.name.c_str());
    }
    if (ImGui::MenuItem("Delete", "Del")) {
        m_pending_delete_id = node.id;
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Copy",  "Ctrl+C")) {}
    if (ImGui::MenuItem("Cut",   "Ctrl+X")) {}
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {}

    ImGui::Separator();

    if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("Node3D"))           {}
        if (ImGui::MenuItem("MeshInstance3D"))   {}
        if (ImGui::MenuItem("Camera3D"))         {}
        if (ImGui::MenuItem("DirectionalLight")) {}
        if (ImGui::MenuItem("PointLight"))       {}
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

EditorNode* SceneTreePanel::FindNode(EditorNode& root, u32 id) {
    if (root.id == id) return &root;
    for (auto& child : root.children) {
        if (auto* found = FindNode(child, id)) return found;
    }
    return nullptr;
}

EditorNode* SceneTreePanel::FindParent(EditorNode& root, u32 child_id) {
    for (auto& child : root.children) {
        if (child.id == child_id) return &root;
        if (auto* found = FindParent(child, child_id)) return found;
    }
    return nullptr;
}

EditorNode SceneTreePanel::DuplicateNode(const EditorNode& src) {
    EditorNode copy = src;
    copy.id   = m_next_id_counter++;
    copy.name = src.name + "_copy";
    for (auto& child : copy.children)
        child = DuplicateNode(child);
    return copy;
}

} // namespace action
