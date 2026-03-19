#include "groups_panel.h"
#include "editor/editor.h"
#include <imgui/imgui.h>
#include <algorithm>

namespace action {

void GroupsPanel::Draw(EditorNode& scene_root, u32& selected_node_id) {
    if (!visible) return;

    if (ImGui::Begin("Groups", &visible)) {
        // Top toolbar: new group input
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
        ImGui::InputTextWithHint("##new_group", "New group name...", m_new_group_name, sizeof(m_new_group_name));
        ImGui::SameLine();
        if (ImGui::Button("Add##grp") && m_new_group_name[0] != '\0') {
            // Create an empty group if it doesn't exist
            std::string name = m_new_group_name;
            if (m_groups.find(name) == m_groups.end()) {
                m_groups[name] = {};
            }
            m_new_group_name[0] = '\0';
        }

        ImGui::Separator();

        // Left column: group list
        float panel_width = ImGui::GetContentRegionAvail().x;
        float left_w = panel_width * 0.45f;
        float right_w = panel_width - left_w - ImGui::GetStyle().ItemSpacing.x;

        if (ImGui::BeginChild("##GroupList", ImVec2(left_w, 0), true)) {
            ImGui::TextDisabled("Groups (%zu)", m_groups.size());
            ImGui::Separator();

            for (auto& [group_name, members] : m_groups) {
                bool is_selected = (group_name == m_selected_group);
                char label[128];
                snprintf(label, sizeof(label), "%s (%zu)", group_name.c_str(), members.size());
                if (ImGui::Selectable(label, is_selected)) {
                    m_selected_group = group_name;
                }

                // Right-click to delete group
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete Group")) {
                        m_groups.erase(group_name);
                        if (m_selected_group == group_name) m_selected_group.clear();
                        ImGui::EndPopup();
                        break; // iterator invalidated
                    }
                    ImGui::EndPopup();
                }
            }

            if (m_groups.empty()) {
                ImGui::TextDisabled("No groups yet.");
                ImGui::TextDisabled("Create one above.");
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right column: group members when a group is selected
        if (ImGui::BeginChild("##GroupMembers", ImVec2(right_w, 0), true)) {
            if (m_selected_group.empty()) {
                ImGui::TextDisabled("Select a group");
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s]", m_selected_group.c_str());
                ImGui::Separator();

                auto it = m_groups.find(m_selected_group);
                if (it != m_groups.end()) {
                    // Collect all scene nodes for name lookup
                    std::vector<EditorNode*> all_nodes;
                    CollectAllNodes(scene_root, all_nodes);

                    // Members list
                    u32 to_remove = 0;
                    for (u32 node_id : it->second) {
                        // Find name
                        const char* node_name = "(unknown)";
                        for (auto* n : all_nodes) {
                            if (n->id == node_id) { node_name = n->name.c_str(); break; }
                        }

                        ImGui::Text("%s", node_name);
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
                        ImGui::PushID(node_id);
                        if (ImGui::SmallButton("x")) {
                            to_remove = node_id;
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove from group");
                        // Click on name to select the node
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {}
                        ImGui::PopID();
                    }
                    if (to_remove != 0) it->second.erase(to_remove);

                    ImGui::Separator();

                    // Add current selection to this group
                    if (selected_node_id != 0) {
                        bool already_in = it->second.count(selected_node_id) > 0;
                        if (already_in) {
                            if (ImGui::Button("Remove Selected")) {
                                it->second.erase(selected_node_id);
                            }
                        } else {
                            if (ImGui::Button("Add Selected")) {
                                it->second.insert(selected_node_id);
                            }
                        }
                    } else {
                        ImGui::TextDisabled("(no node selected)");
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void GroupsPanel::AddNodeToGroup(u32 node_id, const std::string& group_name) {
    m_groups[group_name].insert(node_id);
}

void GroupsPanel::RemoveNodeFromGroup(u32 node_id, const std::string& group_name) {
    auto it = m_groups.find(group_name);
    if (it != m_groups.end()) it->second.erase(node_id);
}

bool GroupsPanel::IsNodeInGroup(u32 node_id, const std::string& group_name) const {
    auto it = m_groups.find(group_name);
    if (it == m_groups.end()) return false;
    return it->second.count(node_id) > 0;
}

std::vector<std::string> GroupsPanel::GetNodeGroups(u32 node_id) const {
    std::vector<std::string> result;
    for (const auto& [name, members] : m_groups) {
        if (members.count(node_id)) result.push_back(name);
    }
    return result;
}

std::vector<u32> GroupsPanel::GetGroupMembers(const std::string& group_name) const {
    auto it = m_groups.find(group_name);
    if (it == m_groups.end()) return {};
    return std::vector<u32>(it->second.begin(), it->second.end());
}

void GroupsPanel::CollectAllNodes(EditorNode& root, std::vector<EditorNode*>& out) {
    out.push_back(&root);
    for (auto& child : root.children) {
        CollectAllNodes(child, out);
    }
}

EditorNode* GroupsPanel::FindNode(EditorNode& root, u32 id) {
    if (root.id == id) return &root;
    for (auto& child : root.children) {
        if (auto* found = FindNode(child, id)) return found;
    }
    return nullptr;
}

} // namespace action
