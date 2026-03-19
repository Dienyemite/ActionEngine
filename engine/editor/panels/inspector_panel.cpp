#include "inspector_panel.h"
#include "editor/editor.h"
#include "gameplay/ecs/ecs.h"
#include "core/logging.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <cstring>

namespace action {

// ------------------------------------------------------------------ helpers --

static void PropRowBegin(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
}

// ------------------------------------------------------------------ Draw ----

void InspectorPanel::Draw(EditorNode* selected_node) {
    if (!visible) return;
    
    if (ImGui::Begin("Inspector", &visible)) {
        if (selected_node == nullptr) {
            ImGui::TextDisabled("No node selected");
            ImGui::Spacing();
            ImGui::TextDisabled("Select a node in the Scene panel");
        } else {
            DrawNodeHeader(*selected_node);
            ImGui::Separator();

            // Search filter
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##insp_search", "Filter properties...", m_search_buf, sizeof(m_search_buf));
            ImGui::Spacing();

            // Sections
            if (ImGui::CollapsingHeader("Node", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawNodeInfo(*selected_node);
            }

            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawTransform(*selected_node);
            }
            
            // Material (mesh objects)
            bool is_mesh = (selected_node->mesh.index != 0 || 
                            selected_node->type == "Cube"           || 
                            selected_node->type == "Sphere"         ||
                            selected_node->type == "Plane"          ||
                            selected_node->type == "Cylinder"       ||
                            selected_node->type == "Capsule"        ||
                            selected_node->type == "MeshInstance3D");
            if (is_mesh) {
                if (SectionVisible("Material") && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::PushID("Material");
                    DrawColor("Color", selected_node->color);
                    ImGui::PopID();
                }
            }
            
            // Type-specific sections
            DrawNodeProperties(*selected_node);
        }
    }
    ImGui::End();
    
    // Process pending delete outside ImGui frame context
    if (m_pending_delete_id != 0 && m_delete_callback) {
        m_delete_callback(m_pending_delete_id);
        m_pending_delete_id = 0;
    }
}

// ---- New: Node info section ----
void InspectorPanel::DrawNodeInfo(EditorNode& node) {
    if (ImGui::BeginTable("##nodeinfo", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("##value",  ImGuiTableColumnFlags_WidthStretch);

        // Node type
        PropRowBegin("Type");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
        ImGui::TextUnformatted(node.type.c_str());
        ImGui::PopStyleColor();

        // Node ID (read-only, useful for scripting)
        PropRowBegin("ID");
        ImGui::TextDisabled("%u", node.id);

        // Visibility
        PropRowBegin("Visible");
        ImGui::Checkbox("##vis", &node.visible);

        ImGui::EndTable();
    }
}

void InspectorPanel::DrawNodeHeader(EditorNode& node) {
    // Type badge
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
    ImGui::TextUnformatted(node.type.c_str());
    ImGui::PopStyleColor();
    
    // Editable name (full-width)
    char name_buffer[128];
    strncpy(name_buffer, node.name.c_str(), sizeof(name_buffer) - 1);
    name_buffer[sizeof(name_buffer) - 1] = '\0';
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##NodeName", name_buffer, sizeof(name_buffer))) {
        node.name = name_buffer;
    }
    
    // Delete button on the right
    float button_width = 64.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.28f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.18f, 0.18f, 1.0f));
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button_width - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button("Delete", ImVec2(button_width, 0))) {
        m_pending_delete_id = node.id;
    }
    ImGui::PopStyleColor(3);
}

bool InspectorPanel::SectionVisible(const char* section_name) const {
    if (m_search_buf[0] == '\0') return true;
    // Simple check: if search string appears in the section name show the section
    std::string s = section_name;
    std::string q = m_search_buf;
    for (char& c : s) c = (char)tolower(c);
    for (char& c : q) c = (char)tolower(c);
    return s.find(q) != std::string::npos;
}

void InspectorPanel::DrawTransform(EditorNode& node) {
    ImGui::PushID("Transform");
    if (ImGui::BeginTable("##xform", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
        DrawVec3("Position", node.position, 0.0f);
        DrawVec3("Rotation", node.rotation, 0.0f);
        DrawVec3("Scale",    node.scale,    1.0f);
        ImGui::EndTable();
    }
    ImGui::PopID();
}

void InspectorPanel::DrawNodeProperties(EditorNode& node) {
    if (node.type == "Camera3D") {
        if (SectionVisible("Camera") && ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##cam", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);

                const char* projections[] = { "Perspective", "Orthographic" };
                PropRowBegin("Projection");
                ImGui::Combo("##proj", &node.cam_projection, projections, IM_ARRAYSIZE(projections));

                PropRowBegin("FOV");
                ImGui::SliderFloat("##fov", &node.cam_fov, 10.0f, 120.0f, "%.1f deg");

                PropRowBegin("Near");
                ImGui::DragFloat("##near", &node.cam_near, 0.01f, 0.001f, 100.0f);

                PropRowBegin("Far");
                ImGui::DragFloat("##far", &node.cam_far, 10.0f, 1.0f, 100000.0f);

                ImGui::EndTable();
            }
        }
    }
    else if (node.type == "DirectionalLight" || node.type == "PointLight" ||
             node.type == "SpotLight"        || node.type == "OmniLight3D") {
        if (SectionVisible("Light") && ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##light", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);

                PropRowBegin("Color");
                float col[3] = {node.light_color.x, node.light_color.y, node.light_color.z};
                if (ImGui::ColorEdit3("##lcol", col))
                    node.light_color = {col[0], col[1], col[2]};

                PropRowBegin("Intensity");
                ImGui::DragFloat("##lint", &node.light_intensity, 0.05f, 0.0f, 100.0f);

                PropRowBegin("Cast Shadows");
                ImGui::Checkbox("##lshad", &node.light_cast_shadows);

                if (node.type != "DirectionalLight") {
                    PropRowBegin("Range");
                    ImGui::DragFloat("##lrange", &node.light_range, 0.1f, 0.1f, 1000.0f);
                }
                if (node.type == "SpotLight") {
                    PropRowBegin("Inner Angle");
                    ImGui::SliderFloat("##linangle", &node.light_inner_angle, 1.0f, node.light_outer_angle, "%.1f°");
                    PropRowBegin("Outer Angle");
                    ImGui::SliderFloat("##loutangle", &node.light_outer_angle, node.light_inner_angle, 90.0f, "%.1f°");
                }

                ImGui::EndTable();
            }
        }
    }
    else if (node.type == "RigidBody3D"  || node.type == "StaticBody3D" ||
             node.type == "CharacterBody3D") {
        if (SectionVisible("Physics") && ImGui::CollapsingHeader("Physics Body", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##phys", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);

                if (node.type == "RigidBody3D") {
                    PropRowBegin("Mass");
                    ImGui::DragFloat("##pmass", &node.phys_mass, 0.1f, 0.001f, 10000.0f);
                }
                PropRowBegin("Friction");
                ImGui::SliderFloat("##pfric", &node.phys_friction, 0.0f, 1.0f);
                PropRowBegin("Bounce");
                ImGui::SliderFloat("##pbounce", &node.phys_bounce, 0.0f, 1.0f);
                if (node.type == "RigidBody3D") {
                    PropRowBegin("Lock Rotation");
                    ImGui::Checkbox("##plockrot", &node.phys_lock_rotation);
                }

                ImGui::EndTable();
            }
        }
    }
    else if (node.type == "GPUParticles3D") {
        if (SectionVisible("Particles") && ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##parts", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
                PropRowBegin("Amount");
                ImGui::DragInt("##pamount", &node.particles_amount, 1.0f, 1, 100000);
                PropRowBegin("Lifetime");
                ImGui::DragFloat("##plife", &node.particles_lifetime, 0.01f, 0.01f, 600.0f, "%.2f s");
                PropRowBegin("Speed Scale");
                ImGui::DragFloat("##pspeed", &node.particles_speed_scale, 0.01f, 0.0f, 100.0f);
                ImGui::EndTable();
            }
        }
    }
    else if (node.type == "AudioStreamPlayer3D") {
        if (SectionVisible("Audio") && ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##aud", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
                PropRowBegin("Volume (dB)");
                ImGui::DragFloat("##avol", &node.audio_volume_db, 0.1f, -80.0f, 24.0f, "%.1f dB");
                PropRowBegin("Pitch");
                ImGui::DragFloat("##apitch", &node.audio_pitch, 0.01f, 0.01f, 16.0f);
                PropRowBegin("Autoplay");
                ImGui::Checkbox("##aauto", &node.audio_autoplay);
                PropRowBegin("Loop");
                ImGui::Checkbox("##aloop", &node.audio_loop);
                ImGui::EndTable();
            }
        }
    }
    else if (node.type == "MeshInstance3D") {
        if (SectionVisible("Mesh") && ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            static char mesh_path[256] = "<none>";
            if (ImGui::BeginTable("##mesh", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
                PropRowBegin("Mesh");
                ImGui::InputText("##mpath", mesh_path, sizeof(mesh_path), ImGuiInputTextFlags_ReadOnly);
                ImGui::EndTable();
            }
        }
        if (SectionVisible("Material") && ImGui::CollapsingHeader("Surface Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("MeshMat");
            if (ImGui::BeginTable("##mmat", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
                static float roughness = 0.5f;
                static float metallic  = 0.0f;
                PropRowBegin("Albedo");
                float col[3] = {node.color.x, node.color.y, node.color.z};
                if (ImGui::ColorEdit3("##alb", col)) node.color = {col[0], col[1], col[2]};
                PropRowBegin("Roughness");
                ImGui::SliderFloat("##rough", &roughness, 0.0f, 1.0f);
                PropRowBegin("Metallic");
                ImGui::SliderFloat("##metal", &metallic, 0.0f, 1.0f);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }
    }

    // Scripts/Components section
    if (SectionVisible("Scripts") && ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool has_scripts = false;

        if (m_ecs && m_scripts && node.entity != INVALID_ENTITY) {
            if (auto* sc = m_ecs->GetComponent<ScriptComponent>(node.entity)) {
                has_scripts = !sc->scripts.empty();

                Script* to_remove = nullptr;
                for (auto& script : sc->scripts) {
                    ImGui::PushID(script.get());

                    bool enabled = script->IsEnabled();
                    if (ImGui::Checkbox("##en", &enabled)) script->SetEnabled(enabled);
                    ImGui::SameLine();

                    if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TextUnformatted(script->GetTypeName());
                    if (!enabled) ImGui::PopStyleColor();

                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f + ImGui::GetCursorPosX());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::SmallButton("X")) to_remove = script.get();
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                }

                if (to_remove) m_scripts->RemoveScript(node.entity, to_remove);
            }
        }

        if (!has_scripts) {
            ImGui::TextDisabled("No scripts attached");
        }

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
        if (ImGui::BeginChild("##scriptlist", ImVec2(280, 200), true)) {
            const auto& types = ScriptFactory::Instance().GetRegisteredTypes();
            for (const auto& type_name : types) {
                if (m_script_filter[0] != '\0' &&
                    type_name.find(m_script_filter) == std::string::npos) continue;
                bool selected = (type_name == m_script_class);
                if (ImGui::Selectable(type_name.c_str(), selected)) {
                    strncpy(m_script_class, type_name.c_str(), sizeof(m_script_class) - 1);
                }
            }
            if (ScriptFactory::Instance().GetRegisteredTypes().empty()) {
                ImGui::TextDisabled("No scripts registered.");
            }
        }
        ImGui::EndChild();

        if (m_script_class[0] != '\0')
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Selected: %s", m_script_class);
        else
            ImGui::TextDisabled("(none selected)");

        ImGui::Separator();
        bool can_attach = (m_script_class[0] != '\0' && m_scripts && node.entity != INVALID_ENTITY);
        if (!can_attach) ImGui::BeginDisabled();
        if (ImGui::Button("Attach", ImVec2(120, 0))) {
            m_scripts->AddScript(node.entity, m_script_class);
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

    // Create New Script modal
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

    // In table context: label col is already set by caller via PropRowBegin
    // When called directly (outside table), use our own layout
    bool in_table = (ImGui::GetCurrentWindow()->DC.CurrentTableIdx >= 0);
    if (in_table) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
    }

    float avail = ImGui::CalcItemWidth();
    if (in_table) avail = ImGui::GetContentRegionAvail().x;
    float btn_w  = ImGui::GetFrameHeight() + 2.0f;
    float field_w = (avail - 3.0f * btn_w - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 0));

    // X
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button("X##rx", ImVec2(btn_w, 0))) { value.x = reset_value; changed = true; }
    ImGui::SameLine(); ImGui::SetNextItemWidth(field_w);
    if (ImGui::DragFloat("##vx", &value.x, 0.1f)) changed = true;
    ImGui::SameLine(); ImGui::PopStyleColor();

    // Y
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.60f, 0.15f, 1.0f));
    if (ImGui::Button("Y##ry", ImVec2(btn_w, 0))) { value.y = reset_value; changed = true; }
    ImGui::SameLine(); ImGui::SetNextItemWidth(field_w);
    if (ImGui::DragFloat("##vy", &value.y, 0.1f)) changed = true;
    ImGui::SameLine(); ImGui::PopStyleColor();

    // Z
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.75f, 1.0f));
    if (ImGui::Button("Z##rz", ImVec2(btn_w, 0))) { value.z = reset_value; changed = true; }
    ImGui::SameLine(); ImGui::SetNextItemWidth(field_w);
    if (ImGui::DragFloat("##vz", &value.z, 0.1f)) changed = true;
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

bool InspectorPanel::DrawFloat(const char* label, float& value, float min, float max) {
    ImGui::PushID(label);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    bool changed = (min != max) ? ImGui::SliderFloat("##v", &value, min, max)
                                : ImGui::DragFloat("##v", &value, 0.1f);
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
    bool changed = ImGui::Checkbox("##v", &value);
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
    bool changed = ImGui::InputText("##v", buffer, sizeof(buffer));
    if (changed) value = buffer;
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
    bool changed = ImGui::ColorEdit3("##c", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel);
    if (changed) color = { col[0], col[1], col[2] };
    ImGui::Columns(1);
    ImGui::PopID();
    return changed;
}

} // namespace action
