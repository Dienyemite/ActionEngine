#include "inspector_panel.h"
#include "editor/editor.h"
#include "gameplay/ecs/ecs.h"
#include "core/logging.h"
#include <imgui/imgui.h>
#include <cstring>

namespace action {

void InspectorPanel::Draw(EditorNode* selected_node) {
    if (!visible) return;
    
    if (ImGui::Begin("Inspector", &visible)) {
        if (selected_node == nullptr) {
            ImGui::TextDisabled("No node selected");
            ImGui::TextDisabled("Select a node in the Scene panel");
        } else {
            DrawNodeHeader(*selected_node);
            
            ImGui::Separator();
            
            // Transform section
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawTransform(*selected_node);
            }
            
            // Material section (for objects with meshes)
            if (selected_node->mesh.index != 0 || selected_node->type == "Cube" || 
                selected_node->type == "Sphere" || selected_node->type == "Plane" ||
                selected_node->type == "MeshInstance3D") {
                if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::PushID("Material");
                    DrawColor("Color", selected_node->color);
                    ImGui::PopID();
                }
            }
            
            // Node-specific properties
            DrawNodeProperties(*selected_node);
        }
    }
    ImGui::End();
    
    // Process pending delete (outside of ImGui window context)
    if (m_pending_delete_id != 0 && m_delete_callback) {
        m_delete_callback(m_pending_delete_id);
        m_pending_delete_id = 0;
    }
}

void InspectorPanel::DrawNodeHeader(EditorNode& node) {
    // Node type
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
    ImGui::Text("%s", node.type.c_str());
    ImGui::PopStyleColor();
    
    // Editable name
    char name_buffer[128];
    strncpy(name_buffer, node.name.c_str(), sizeof(name_buffer) - 1);
    name_buffer[sizeof(name_buffer) - 1] = '\0';
    
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##NodeName", name_buffer, sizeof(name_buffer))) {
        node.name = name_buffer;
    }
    
    // Visibility checkbox and Delete button on same line
    ImGui::Checkbox("Visible", &node.visible);
    ImGui::SameLine();
    
    // Delete button (red, on the right side)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    
    // Position button on the right
    float button_width = 60.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button_width - ImGui::GetStyle().WindowPadding.x);
    
    if (ImGui::Button("Delete", ImVec2(button_width, 0))) {
        // Store the ID to delete - will be processed via callback
        m_pending_delete_id = node.id;
    }
    ImGui::PopStyleColor(3);
}

void InspectorPanel::DrawTransform(EditorNode& node) {
    ImGui::PushID("Transform");
    
    DrawVec3("Position", node.position, 0.0f);
    DrawVec3("Rotation", node.rotation, 0.0f);
    DrawVec3("Scale", node.scale, 1.0f);
    
    ImGui::PopID();
}

void InspectorPanel::DrawNodeProperties(EditorNode& node) {
    // Draw type-specific properties
    
    if (node.type == "Camera3D") {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            static float fov = 75.0f;
            static float near_plane = 0.1f;
            static float far_plane = 2000.0f;
            static int projection = 0;
            
            const char* projections[] = { "Perspective", "Orthographic" };
            ImGui::Combo("Projection", &projection, projections, IM_ARRAYSIZE(projections));
            
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.1f");
            ImGui::DragFloat("Near", &near_plane, 0.01f, 0.001f, 10.0f);
            ImGui::DragFloat("Far", &far_plane, 10.0f, 10.0f, 10000.0f);
        }
    }
    else if (node.type == "DirectionalLight" || node.type == "PointLight" || node.type == "SpotLight") {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            static vec3 color = {1.0f, 0.95f, 0.9f};
            static float intensity = 1.0f;
            static bool shadows = true;
            
            DrawColor("Color", color);
            ImGui::SliderFloat("Intensity", &intensity, 0.0f, 10.0f);
            ImGui::Checkbox("Cast Shadows", &shadows);
            
            if (node.type == "PointLight" || node.type == "SpotLight") {
                static float range = 10.0f;
                ImGui::DragFloat("Range", &range, 0.1f, 0.1f, 100.0f);
            }
            
            if (node.type == "SpotLight") {
                static float inner_angle = 30.0f;
                static float outer_angle = 45.0f;
                ImGui::SliderFloat("Inner Angle", &inner_angle, 1.0f, outer_angle);
                ImGui::SliderFloat("Outer Angle", &outer_angle, inner_angle, 90.0f);
            }
        }
    }
    else if (node.type == "MeshInstance3D") {
        static char mesh_path[256] = "<none>";
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Mesh", mesh_path, sizeof(mesh_path), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("...##mesh")) {
                m_show_mesh_browser = true;
            }
        }
        
        // Mesh browser modal
        if (m_show_mesh_browser) ImGui::OpenPopup("##MeshBrowser");
        if (ImGui::BeginPopup("##MeshBrowser")) {
            ImGui::Text("Select Mesh");
            ImGui::Separator();
            ImGui::SetNextItemWidth(200);
            ImGui::InputTextWithHint("##mf", "Filter...", m_mesh_filter, sizeof(m_mesh_filter));
            
            // Common built-in shapes
            static const char* k_builtins[] = { "Cube", "Sphere", "Plane", "Cylinder", "Capsule" };
            for (const char* name : k_builtins) {
                if (m_mesh_filter[0] != '\0' &&
                    std::strstr(name, m_mesh_filter) == nullptr) continue;
                if (ImGui::Selectable(name)) {
                    strncpy(mesh_path, name, sizeof(mesh_path) - 1);
                    m_show_mesh_browser = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::Separator();
            if (ImGui::Button("Cancel")) {
                m_show_mesh_browser = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        m_show_mesh_browser = false;  // reset trigger each frame
        
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            static vec3 albedo = {0.8f, 0.8f, 0.8f};
            static float roughness = 0.5f;
            static float metallic = 0.0f;
            
            DrawColor("Albedo", albedo);
            ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
        }
    }
    
    // Scripts/Components section
    if (ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool has_scripts = false;

        if (m_ecs && m_scripts && node.entity != INVALID_ENTITY) {
            if (auto* sc = m_ecs->GetComponent<ScriptComponent>(node.entity)) {
                has_scripts = !sc->scripts.empty();

                // List attached scripts
                Script* to_remove = nullptr;
                for (auto& script : sc->scripts) {
                    ImGui::PushID(script.get());

                    // Enabled toggle
                    bool enabled = script->IsEnabled();
                    if (ImGui::Checkbox("##en", &enabled)) {
                        script->SetEnabled(enabled);
                    }
                    ImGui::SameLine();

                    // Script type name, greyed out if disabled
                    if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TextUnformatted(script->GetTypeName());
                    if (!enabled) ImGui::PopStyleColor();

                    // Remove button
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f + ImGui::GetCursorPosX());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::SmallButton("X")) {
                        to_remove = script.get();
                    }
                    ImGui::PopStyleColor(2);

                    ImGui::PopID();
                }

                // Deferred removal (avoid invalidating iterator)
                if (to_remove) {
                    m_scripts->RemoveScript(node.entity, to_remove);
                }
            }
        }

        if (!has_scripts) {
            ImGui::TextDisabled("No scripts attached");
        }

        // Add Script button
        ImGui::Spacing();
        float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("+ Add Script", ImVec2(btn_w, 0))) {
            m_show_script_dialog = true;
            m_script_class[0]  = '\0';
            m_script_filter[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("+ New Script", ImVec2(btn_w, 0))) {
            m_show_create_script_dialog = true;
            m_new_script_name[0] = '\0';
        }
    }

    // Add Script modal
    if (m_show_script_dialog) {
        ImGui::OpenPopup("Add Script");
        m_show_script_dialog = false;
    }
    if (ImGui::BeginPopupModal("Add Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a script to attach to '%s':", node.name.c_str());
        ImGui::Separator();

        ImGui::SetNextItemWidth(280);
        ImGui::InputTextWithHint("##sf", "Filter...", m_script_filter, sizeof(m_script_filter));

        // List registered script types
        if (ImGui::BeginChild("##scriptlist", ImVec2(280, 200), true)) {
            const auto& types = ScriptFactory::Instance().GetRegisteredTypes();
            for (const auto& type_name : types) {
                if (m_script_filter[0] != '\0' &&
                    type_name.find(m_script_filter) == std::string::npos) continue;

                bool selected = (type_name == m_script_class);
                if (ImGui::Selectable(type_name.c_str(), selected)) {
                    strncpy(m_script_class, type_name.c_str(), sizeof(m_script_class) - 1);
                    m_script_class[sizeof(m_script_class) - 1] = '\0';
                }
            }
            if (ScriptFactory::Instance().GetRegisteredTypes().empty()) {
                ImGui::TextDisabled("No scripts registered.");
                ImGui::TextDisabled("Call REGISTER_SCRIPT(MyScript)");
                ImGui::TextDisabled("before engine.Run().");
            }
        }
        ImGui::EndChild();

        // Show selection
        if (m_script_class[0] != '\0') {
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Selected: %s", m_script_class);
        } else {
            ImGui::TextDisabled("(none selected)");
        }

        ImGui::Separator();

        bool can_attach = (m_script_class[0] != '\0' && m_scripts &&
                           node.entity != INVALID_ENTITY);
        if (!can_attach) ImGui::BeginDisabled();
        if (ImGui::Button("Attach", ImVec2(120, 0))) {
            m_scripts->AddScript(node.entity, m_script_class);
            LOG_INFO("[Inspector] Attached script '{}' to entity {}", m_script_class, node.entity);
            m_script_class[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        if (!can_attach) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_script_class[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Create New Script modal ----
    if (m_show_create_script_dialog) {
        ImGui::OpenPopup("Create New Script");
        m_show_create_script_dialog = false;
    }
    if (ImGui::BeginPopupModal("Create New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Class name:");
        ImGui::SetNextItemWidth(260);
        bool enter = ImGui::InputText("##newscript", m_new_script_name, sizeof(m_new_script_name),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        ImGui::TextDisabled("A C++ header will be created in game/scripts/");
        ImGui::TextDisabled("Edit it in VS Code, then rebuild the project.");
        ImGui::Separator();

        bool valid = (m_new_script_name[0] != '\0');
        if (!valid) ImGui::BeginDisabled();
        if (ImGui::Button("Create & Open in VS Code", ImVec2(200, 0)) || (enter && valid)) {
            if (m_create_script_cb) m_create_script_cb(m_new_script_name);
            ImGui::CloseCurrentPopup();
        }
        if (!valid) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel##cs", ImVec2(90, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

bool InspectorPanel::DrawVec3(const char* label, vec3& value, float reset_value) {
    bool changed = false;
    
    ImGui::PushID(label);
    
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80);
    
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    
    float width = (ImGui::CalcItemWidth() - 60) / 3.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    
    float line_height = ImGui::GetFrameHeight();
    ImVec2 button_size = { line_height + 3.0f, line_height };
    
    // X component
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("X", button_size)) {
        value.x = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(width);
    if (ImGui::DragFloat("##X", &value.x, 0.1f)) {
        changed = true;
    }
    ImGui::SameLine();
    
    // Y component
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("Y", button_size)) {
        value.y = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(width);
    if (ImGui::DragFloat("##Y", &value.y, 0.1f)) {
        changed = true;
    }
    ImGui::SameLine();
    
    // Z component
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    if (ImGui::Button("Z", button_size)) {
        value.z = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(width);
    if (ImGui::DragFloat("##Z", &value.z, 0.1f)) {
        changed = true;
    }
    
    ImGui::PopStyleVar();
    ImGui::Columns(1);
    
    ImGui::PopID();
    
    return changed;
}

bool InspectorPanel::DrawFloat(const char* label, float& value, float min, float max) {
    ImGui::PushID(label);
    
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    
    bool changed;
    if (min != max) {
        changed = ImGui::SliderFloat("##value", &value, min, max);
    } else {
        changed = ImGui::DragFloat("##value", &value, 0.1f);
    }
    
    ImGui::Columns(1);
    ImGui::PopID();
    
    return changed;
}

bool InspectorPanel::DrawBool(const char* label, bool& value) {
    ImGui::PushID(label);
    
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    
    bool changed = ImGui::Checkbox("##value", &value);
    
    ImGui::Columns(1);
    ImGui::PopID();
    
    return changed;
}

bool InspectorPanel::DrawString(const char* label, std::string& value) {
    ImGui::PushID(label);
    
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    
    char buffer[256];
    strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    bool changed = ImGui::InputText("##value", buffer, sizeof(buffer));
    if (changed) {
        value = buffer;
    }
    
    ImGui::Columns(1);
    ImGui::PopID();
    
    return changed;
}

bool InspectorPanel::DrawColor(const char* label, vec3& color) {
    ImGui::PushID(label);
    
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    
    float col[3] = { color.x, color.y, color.z };
    bool changed = ImGui::ColorEdit3("##color", col, ImGuiColorEditFlags_NoInputs);
    if (changed) {
        color = { col[0], col[1], col[2] };
    }
    
    ImGui::Columns(1);
    ImGui::PopID();
    
    return changed;
}

} // namespace action
