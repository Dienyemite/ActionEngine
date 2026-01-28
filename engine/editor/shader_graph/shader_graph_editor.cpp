#include "shader_graph_editor.h"
#include "render/renderer.h"
#include "core/logging.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <fstream>
#include <cmath>

namespace action {

void ShaderGraphEditor::Initialize() {
    m_graph.Initialize();
    LOG_INFO("ShaderGraphEditor initialized");
}

void ShaderGraphEditor::NewGraph() {
    m_graph.Clear();
    m_graph.Initialize();
    m_canvas_offset = {0, 0};
    m_canvas_zoom = 1.0f;
}

bool ShaderGraphEditor::LoadGraph(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader graph: {}", path);
        return false;
    }
    
    std::string json((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    
    if (!m_graph.FromJson(json)) {
        LOG_ERROR("Failed to parse shader graph: {}", path);
        return false;
    }
    
    LOG_INFO("Loaded shader graph: {}", path);
    return true;
}

bool ShaderGraphEditor::SaveGraph(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to save shader graph: {}", path);
        return false;
    }
    
    file << m_graph.ToJson();
    LOG_INFO("Saved shader graph: {}", path);
    return true;
}

void ShaderGraphEditor::Draw(Renderer& renderer) {
    if (!visible) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (ImGui::Begin("Shader Graph", &visible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        // Store canvas info
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        m_canvas_screen_pos = {pos.x, pos.y};
        m_canvas_size = {size.x, size.y};
        
        // Draw toolbar first
        ImGui::PopStyleVar();  // Reset padding for toolbar
        DrawToolbar();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        ImGui::Separator();
        
        // Update canvas position after toolbar
        pos = ImGui::GetCursorScreenPos();
        size = ImGui::GetContentRegionAvail();
        m_canvas_screen_pos = {pos.x, pos.y};
        m_canvas_size = {size.x, size.y};
        
        // Draw canvas
        DrawCanvas();
    }
    ImGui::End();
    
    ImGui::PopStyleVar();
    
    // Draw code preview window
    if (m_show_code_preview) {
        DrawCodePreview();
    }
}

void ShaderGraphEditor::DrawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    
    if (ImGui::Button("New")) {
        NewGraph();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveGraph("materials/" + m_graph.name + ".shadergraph");
    }
    ImGui::SameLine();
    
    ImGui::Text("|");
    ImGui::SameLine();
    
    // Material name
    char name_buffer[128];
    strncpy(name_buffer, m_graph.name.c_str(), sizeof(name_buffer) - 1);
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##MaterialName", name_buffer, sizeof(name_buffer))) {
        m_graph.name = name_buffer;
    }
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    ImGui::Checkbox("Grid", &m_show_grid);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_snap_to_grid);
    ImGui::SameLine();
    ImGui::Checkbox("Minimap", &m_show_minimap);
    ImGui::SameLine();
    ImGui::Checkbox("Code", &m_show_code_preview);
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    // Zoom controls
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::SliderFloat("##Zoom", &m_canvas_zoom, 0.25f, 2.0f, "%.2f")) {
        m_canvas_zoom = std::clamp(m_canvas_zoom, 0.25f, 2.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("1:1")) {
        m_canvas_zoom = 1.0f;
    }
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    // Stats
    ImGui::Text("Nodes: %zu  Links: %zu", m_graph.GetNodes().size(), m_graph.GetLinks().size());
    
    ImGui::PopStyleVar();
}

void ShaderGraphEditor::DrawCanvas() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 canvas_p0(m_canvas_screen_pos.x, m_canvas_screen_pos.y);
    ImVec2 canvas_p1(canvas_p0.x + m_canvas_size.x, canvas_p0.y + m_canvas_size.y);
    
    // Background
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 30, 35, 255));
    
    // Grid
    if (m_show_grid) {
        float grid_step = m_grid_size * m_canvas_zoom;
        float offset_x = fmodf(m_canvas_offset.x * m_canvas_zoom, grid_step);
        float offset_y = fmodf(m_canvas_offset.y * m_canvas_zoom, grid_step);
        
        ImU32 grid_color = IM_COL32(50, 50, 55, 255);
        ImU32 grid_color_major = IM_COL32(60, 60, 65, 255);
        
        int line_count = 0;
        for (float x = offset_x; x < m_canvas_size.x; x += grid_step) {
            ImU32 color = (line_count % 5 == 0) ? grid_color_major : grid_color;
            draw_list->AddLine(
                ImVec2(canvas_p0.x + x, canvas_p0.y),
                ImVec2(canvas_p0.x + x, canvas_p1.y),
                color
            );
            line_count++;
        }
        line_count = 0;
        for (float y = offset_y; y < m_canvas_size.y; y += grid_step) {
            ImU32 color = (line_count % 5 == 0) ? grid_color_major : grid_color;
            draw_list->AddLine(
                ImVec2(canvas_p0.x, canvas_p0.y + y),
                ImVec2(canvas_p1.x, canvas_p0.y + y),
                color
            );
            line_count++;
        }
    }
    
    // Clip content to canvas
    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    
    // Draw links (behind nodes)
    DrawLinks();
    
    // Draw in-progress link
    if (m_dragging_link) {
        vec2 start_pos = m_link_end_pos;
        if (m_link_start_pin != 0) {
            if (ShaderNode* node = m_graph.FindPinOwner(m_link_start_pin)) {
                if (ShaderPin* pin = m_graph.FindPin(m_link_start_pin)) {
                    start_pos = GetPinPosition(*node, *pin);
                }
            }
        }
        
        ImVec2 p1(start_pos.x, start_pos.y);
        ImVec2 p2(m_link_end_pos.x, m_link_end_pos.y);
        
        // Convert to screen coords
        p1 = ImVec2(canvas_p0.x + (p1.x + m_canvas_offset.x) * m_canvas_zoom,
                    canvas_p0.y + (p1.y + m_canvas_offset.y) * m_canvas_zoom);
        p2 = ImVec2(canvas_p0.x + (p2.x + m_canvas_offset.x) * m_canvas_zoom,
                    canvas_p0.y + (p2.y + m_canvas_offset.y) * m_canvas_zoom);
        
        float dx = (p2.x - p1.x) * 0.5f;
        draw_list->AddBezierCubic(p1, ImVec2(p1.x + dx, p1.y), ImVec2(p2.x - dx, p2.y), p2,
                                   IM_COL32(200, 200, 200, 200), 2.0f);
    }
    
    // Draw nodes
    DrawNodes();
    
    // Draw selection box
    if (m_box_selecting) {
        ImVec2 box_min(std::min(m_selection_start.x, m_link_end_pos.x),
                       std::min(m_selection_start.y, m_link_end_pos.y));
        ImVec2 box_max(std::max(m_selection_start.x, m_link_end_pos.x),
                       std::max(m_selection_start.y, m_link_end_pos.y));
        
        draw_list->AddRectFilled(box_min, box_max, IM_COL32(100, 150, 200, 50));
        draw_list->AddRect(box_min, box_max, IM_COL32(100, 150, 200, 200));
    }
    
    draw_list->PopClipRect();
    
    // Minimap
    if (m_show_minimap) {
        DrawMinimap();
    }
    
    // Handle input
    ImGui::SetCursorScreenPos(canvas_p0);
    ImGui::InvisibleButton("canvas", ImVec2(m_canvas_size.x, m_canvas_size.y),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                           ImGuiButtonFlags_MouseButtonMiddle);
    
    bool canvas_hovered = ImGui::IsItemHovered();
    bool canvas_active = ImGui::IsItemActive();
    
    if (canvas_hovered || canvas_active) {
        HandleCanvasInput();
    }
    
    // Context menus
    DrawContextMenu();
    DrawNodeContextMenu();
}

void ShaderGraphEditor::DrawNodes() {
    // Sort by selection for draw order (selected on top)
    std::vector<ShaderNode*> sorted_nodes;
    for (auto& node : m_graph.GetNodes()) {
        sorted_nodes.push_back(&node);
    }
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
              [](const ShaderNode* a, const ShaderNode* b) { return !a->selected && b->selected; });
    
    for (auto* node : sorted_nodes) {
        DrawNode(*node);
    }
}

void ShaderGraphEditor::DrawNode(ShaderNode& node) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Convert to screen coords
    vec2 node_screen_pos = CanvasToScreen(node.position);
    vec2 node_screen_size = node.size * m_canvas_zoom;
    
    ImVec2 node_min(node_screen_pos.x, node_screen_pos.y);
    ImVec2 node_max(node_min.x + node_screen_size.x, node_min.y + node_screen_size.y);
    
    // Clipping check
    if (node_max.x < m_canvas_screen_pos.x || node_min.x > m_canvas_screen_pos.x + m_canvas_size.x ||
        node_max.y < m_canvas_screen_pos.y || node_min.y > m_canvas_screen_pos.y + m_canvas_size.y) {
        return;  // Node is off-screen
    }
    
    float header_height = 24.0f * m_canvas_zoom;
    float rounding = 6.0f * m_canvas_zoom;
    float pin_radius = 6.0f * m_canvas_zoom;
    
    // Node shadow
    draw_list->AddRectFilled(
        ImVec2(node_min.x + 4, node_min.y + 4),
        ImVec2(node_max.x + 4, node_max.y + 4),
        IM_COL32(0, 0, 0, 80), rounding
    );
    
    // Node body
    ImU32 body_color = node.selected ? IM_COL32(60, 60, 70, 255) : IM_COL32(45, 45, 50, 255);
    draw_list->AddRectFilled(node_min, node_max, body_color, rounding);
    
    // Header
    draw_list->AddRectFilled(
        node_min,
        ImVec2(node_max.x, node_min.y + header_height),
        node.GetHeaderColor(), rounding, ImDrawFlags_RoundCornersTop
    );
    
    // Border
    ImU32 border_color = node.selected ? IM_COL32(100, 150, 200, 255) : IM_COL32(70, 70, 80, 255);
    if (m_hovered_node_id == node.id && !node.selected) {
        border_color = IM_COL32(120, 120, 130, 255);
    }
    draw_list->AddRect(node_min, node_max, border_color, rounding, 0, 1.5f);
    
    // Title
    float font_scale = m_canvas_zoom;
    ImVec2 title_pos(node_min.x + 8 * m_canvas_zoom, node_min.y + 4 * m_canvas_zoom);
    draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize() * font_scale,
                       title_pos, IM_COL32(220, 220, 220, 255), node.title.c_str());
    
    // Input pins (left side)
    float pin_start_y = node_min.y + header_height + 8 * m_canvas_zoom;
    float pin_spacing = 20.0f * m_canvas_zoom;
    
    for (size_t i = 0; i < node.inputs.size(); ++i) {
        auto& pin = node.inputs[i];
        ImVec2 pin_pos(node_min.x, pin_start_y + i * pin_spacing);
        
        // Pin circle
        ImU32 pin_color = pin.GetTypeColor();
        if (m_hovered_pin_id == pin.id) {
            pin_color = IM_COL32(255, 255, 255, 255);
        }
        draw_list->AddCircleFilled(pin_pos, pin_radius, pin_color);
        if (pin.connected) {
            draw_list->AddCircle(pin_pos, pin_radius, IM_COL32(255, 255, 255, 200), 0, 1.5f);
        }
        
        // Pin label
        ImVec2 label_pos(pin_pos.x + pin_radius + 4 * m_canvas_zoom, pin_pos.y - 6 * m_canvas_zoom);
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize() * font_scale * 0.9f,
                           label_pos, IM_COL32(180, 180, 180, 255), pin.name.c_str());
    }
    
    // Output pins (right side)
    for (size_t i = 0; i < node.outputs.size(); ++i) {
        auto& pin = node.outputs[i];
        ImVec2 pin_pos(node_max.x, pin_start_y + i * pin_spacing);
        
        // Pin circle
        ImU32 pin_color = pin.GetTypeColor();
        if (m_hovered_pin_id == pin.id) {
            pin_color = IM_COL32(255, 255, 255, 255);
        }
        draw_list->AddCircleFilled(pin_pos, pin_radius, pin_color);
        if (pin.connected) {
            draw_list->AddCircle(pin_pos, pin_radius, IM_COL32(255, 255, 255, 200), 0, 1.5f);
        }
        
        // Pin label (right-aligned)
        ImVec2 label_size = ImGui::CalcTextSize(pin.name.c_str());
        ImVec2 label_pos(pin_pos.x - pin_radius - 4 * m_canvas_zoom - label_size.x * font_scale * 0.9f,
                         pin_pos.y - 6 * m_canvas_zoom);
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize() * font_scale * 0.9f,
                           label_pos, IM_COL32(180, 180, 180, 255), pin.name.c_str());
    }
}

void ShaderGraphEditor::DrawLinks() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    for (const auto& link : m_graph.GetLinks()) {
        DrawLink(link);
    }
}

void ShaderGraphEditor::DrawLink(const ShaderLink& link) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ShaderNode* from_node = m_graph.GetNode(link.from_node);
    ShaderNode* to_node = m_graph.GetNode(link.to_node);
    ShaderPin* from_pin = m_graph.FindPin(link.from_pin);
    ShaderPin* to_pin = m_graph.FindPin(link.to_pin);
    
    if (!from_node || !to_node || !from_pin || !to_pin) return;
    
    vec2 from_pos = GetPinPosition(*from_node, *from_pin);
    vec2 to_pos = GetPinPosition(*to_node, *to_pin);
    
    // Convert to screen coords
    ImVec2 p1 = ImVec2(m_canvas_screen_pos.x + (from_pos.x + m_canvas_offset.x) * m_canvas_zoom,
                       m_canvas_screen_pos.y + (from_pos.y + m_canvas_offset.y) * m_canvas_zoom);
    ImVec2 p2 = ImVec2(m_canvas_screen_pos.x + (to_pos.x + m_canvas_offset.x) * m_canvas_zoom,
                       m_canvas_screen_pos.y + (to_pos.y + m_canvas_offset.y) * m_canvas_zoom);
    
    // Control points for bezier curve
    float dx = (p2.x - p1.x) * 0.5f;
    dx = std::max(dx, 50.0f * m_canvas_zoom);
    
    ImU32 color = from_pin->GetTypeColor();
    float thickness = 2.0f * m_canvas_zoom;
    
    draw_list->AddBezierCubic(p1, ImVec2(p1.x + dx, p1.y), ImVec2(p2.x - dx, p2.y), p2,
                               color, thickness);
}

void ShaderGraphEditor::DrawContextMenu() {
    if (m_show_context_menu) {
        ImGui::OpenPopup("CanvasContextMenu");
        m_show_context_menu = false;
    }
    
    if (ImGui::BeginPopup("CanvasContextMenu")) {
        vec2 spawn_pos = ScreenToCanvas(m_context_menu_pos);
        
        if (ImGui::BeginMenu("Add Node")) {
            if (ImGui::BeginMenu("Constants")) {
                if (ImGui::MenuItem("Float")) { m_graph.AddNode(ShaderNodeType::ConstantFloat, spawn_pos); }
                if (ImGui::MenuItem("Vector2")) { m_graph.AddNode(ShaderNodeType::ConstantVec2, spawn_pos); }
                if (ImGui::MenuItem("Vector3")) { m_graph.AddNode(ShaderNodeType::ConstantVec3, spawn_pos); }
                if (ImGui::MenuItem("Vector4")) { m_graph.AddNode(ShaderNodeType::ConstantVec4, spawn_pos); }
                if (ImGui::MenuItem("Color")) { m_graph.AddNode(ShaderNodeType::ConstantColor, spawn_pos); }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Inputs")) {
                if (ImGui::MenuItem("Texture Sample")) { m_graph.AddNode(ShaderNodeType::TextureSample, spawn_pos); }
                if (ImGui::MenuItem("Time")) { m_graph.AddNode(ShaderNodeType::Time, spawn_pos); }
                if (ImGui::MenuItem("Vertex Position")) { m_graph.AddNode(ShaderNodeType::VertexPosition, spawn_pos); }
                if (ImGui::MenuItem("Vertex Normal")) { m_graph.AddNode(ShaderNodeType::VertexNormal, spawn_pos); }
                if (ImGui::MenuItem("UV")) { m_graph.AddNode(ShaderNodeType::VertexUV, spawn_pos); }
                if (ImGui::MenuItem("Vertex Color")) { m_graph.AddNode(ShaderNodeType::VertexColor, spawn_pos); }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Math")) {
                if (ImGui::MenuItem("Add")) { m_graph.AddNode(ShaderNodeType::Add, spawn_pos); }
                if (ImGui::MenuItem("Subtract")) { m_graph.AddNode(ShaderNodeType::Subtract, spawn_pos); }
                if (ImGui::MenuItem("Multiply")) { m_graph.AddNode(ShaderNodeType::Multiply, spawn_pos); }
                if (ImGui::MenuItem("Divide")) { m_graph.AddNode(ShaderNodeType::Divide, spawn_pos); }
                ImGui::Separator();
                if (ImGui::MenuItem("Sin")) { m_graph.AddNode(ShaderNodeType::Sin, spawn_pos); }
                if (ImGui::MenuItem("Cos")) { m_graph.AddNode(ShaderNodeType::Cos, spawn_pos); }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Vector")) {
                if (ImGui::MenuItem("Dot Product")) { m_graph.AddNode(ShaderNodeType::Dot, spawn_pos); }
                if (ImGui::MenuItem("Cross Product")) { m_graph.AddNode(ShaderNodeType::Cross, spawn_pos); }
                if (ImGui::MenuItem("Normalize")) { m_graph.AddNode(ShaderNodeType::Normalize, spawn_pos); }
                if (ImGui::MenuItem("Length")) { m_graph.AddNode(ShaderNodeType::Length, spawn_pos); }
                ImGui::Separator();
                if (ImGui::MenuItem("Lerp")) { m_graph.AddNode(ShaderNodeType::Lerp, spawn_pos); }
                if (ImGui::MenuItem("Clamp")) { m_graph.AddNode(ShaderNodeType::Clamp, spawn_pos); }
                if (ImGui::MenuItem("Saturate")) { m_graph.AddNode(ShaderNodeType::Saturate, spawn_pos); }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Utility")) {
                if (ImGui::MenuItem("Split")) { m_graph.AddNode(ShaderNodeType::Split, spawn_pos); }
                if (ImGui::MenuItem("Combine")) { m_graph.AddNode(ShaderNodeType::Combine, spawn_pos); }
                if (ImGui::MenuItem("Fresnel")) { m_graph.AddNode(ShaderNodeType::Fresnel, spawn_pos); }
                ImGui::Separator();
                if (ImGui::MenuItem("Comment")) { m_graph.AddNode(ShaderNodeType::Comment, spawn_pos); }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Select All")) {
            for (auto& node : m_graph.GetNodes()) {
                node.selected = true;
            }
        }
        
        ImGui::EndPopup();
    }
}

void ShaderGraphEditor::DrawNodeContextMenu() {
    if (m_show_node_context_menu) {
        ImGui::OpenPopup("NodeContextMenu");
        m_show_node_context_menu = false;
    }
    
    if (ImGui::BeginPopup("NodeContextMenu")) {
        if (ImGui::MenuItem("Delete", "Del")) {
            auto selected = m_graph.GetSelectedNodeIds();
            for (u32 id : selected) {
                m_graph.DeleteNode(id);
            }
        }
        
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            // TODO: Implement duplication
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Disconnect All")) {
            auto selected = m_graph.GetSelectedNodeIds();
            for (u32 id : selected) {
                if (ShaderNode* node = m_graph.GetNode(id)) {
                    for (auto& pin : node->inputs) {
                        m_graph.DeleteLinksToPin(pin.id);
                    }
                    for (auto& pin : node->outputs) {
                        m_graph.DeleteLinksToPin(pin.id);
                    }
                }
            }
        }
        
        ImGui::EndPopup();
    }
}

void ShaderGraphEditor::DrawMinimap() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    float minimap_size = 150.0f;
    float padding = 10.0f;
    
    ImVec2 minimap_pos(
        m_canvas_screen_pos.x + m_canvas_size.x - minimap_size - padding,
        m_canvas_screen_pos.y + m_canvas_size.y - minimap_size - padding
    );
    
    // Background
    draw_list->AddRectFilled(
        minimap_pos,
        ImVec2(minimap_pos.x + minimap_size, minimap_pos.y + minimap_size),
        IM_COL32(20, 20, 25, 200),
        4.0f
    );
    draw_list->AddRect(
        minimap_pos,
        ImVec2(minimap_pos.x + minimap_size, minimap_pos.y + minimap_size),
        IM_COL32(60, 60, 70, 255),
        4.0f
    );
    
    // Calculate bounds of all nodes
    if (m_graph.GetNodes().empty()) return;
    
    vec2 min_pos{99999, 99999};
    vec2 max_pos{-99999, -99999};
    
    for (const auto& node : m_graph.GetNodes()) {
        min_pos.x = std::min(min_pos.x, node.position.x);
        min_pos.y = std::min(min_pos.y, node.position.y);
        max_pos.x = std::max(max_pos.x, node.position.x + node.size.x);
        max_pos.y = std::max(max_pos.y, node.position.y + node.size.y);
    }
    
    // Add margin
    vec2 graph_size = max_pos - min_pos + vec2{100, 100};
    float scale = std::min(minimap_size / graph_size.x, minimap_size / graph_size.y) * 0.8f;
    
    // Draw nodes in minimap
    for (const auto& node : m_graph.GetNodes()) {
        vec2 node_pos = (node.position - min_pos + vec2{50, 50}) * scale;
        vec2 node_size = node.size * scale;
        
        ImU32 color = node.selected ? IM_COL32(100, 150, 200, 255) : IM_COL32(80, 80, 90, 255);
        draw_list->AddRectFilled(
            ImVec2(minimap_pos.x + node_pos.x, minimap_pos.y + node_pos.y),
            ImVec2(minimap_pos.x + node_pos.x + node_size.x, minimap_pos.y + node_pos.y + node_size.y),
            color
        );
    }
    
    // Draw viewport rectangle
    vec2 view_min = (vec2{0, 0} - m_canvas_offset - min_pos + vec2{50, 50}) * scale;
    vec2 view_max = view_min + (m_canvas_size * (1.0f / m_canvas_zoom)) * scale;
    
    draw_list->AddRect(
        ImVec2(minimap_pos.x + view_min.x, minimap_pos.y + view_min.y),
        ImVec2(minimap_pos.x + view_max.x, minimap_pos.y + view_max.y),
        IM_COL32(200, 200, 200, 200)
    );
}

void ShaderGraphEditor::DrawCodePreview() {
    if (ImGui::Begin("Generated Shader Code", &m_show_code_preview)) {
        std::string code = m_graph.GenerateGLSL();
        
        ImGui::BeginChild("CodeScroll", ImVec2(0, 0), true);
        ImGui::TextUnformatted(code.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}

void ShaderGraphEditor::HandleCanvasInput() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Update hovered state
    vec2 mouse_pos = ScreenToCanvas({io.MousePos.x, io.MousePos.y});
    m_hovered_node_id = 0;
    m_hovered_pin_id = 0;
    
    ShaderNode* hovered_node = nullptr;
    if (ShaderPin* pin = HitTestPin(mouse_pos, &hovered_node)) {
        m_hovered_pin_id = pin->id;
        if (hovered_node) m_hovered_node_id = hovered_node->id;
    } else if (ShaderNode* node = HitTestNode(mouse_pos)) {
        m_hovered_node_id = node->id;
    }
    
    // Middle mouse button for panning
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        m_canvas_offset.x += io.MouseDelta.x / m_canvas_zoom;
        m_canvas_offset.y += io.MouseDelta.y / m_canvas_zoom;
    }
    
    // Scroll for zoom
    if (io.MouseWheel != 0.0f) {
        float zoom_delta = io.MouseWheel * 0.1f;
        float new_zoom = std::clamp(m_canvas_zoom + zoom_delta, 0.25f, 2.0f);
        
        // Zoom towards mouse position
        vec2 mouse_before = ScreenToCanvas({io.MousePos.x, io.MousePos.y});
        m_canvas_zoom = new_zoom;
        vec2 mouse_after = ScreenToCanvas({io.MousePos.x, io.MousePos.y});
        
        m_canvas_offset = m_canvas_offset + (mouse_after - mouse_before);
    }
    
    // Left click for selection/link creation
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (m_hovered_pin_id != 0) {
            // Start link drag
            m_dragging_link = true;
            m_link_start_pin = m_hovered_pin_id;
            ShaderPin* pin = m_graph.FindPin(m_hovered_pin_id);
            m_link_from_input = (pin && pin->direction == ShaderPinDirection::Input);
        } else if (m_hovered_node_id != 0) {
            // Select node
            if (!io.KeyCtrl && !io.KeyShift) {
                if (!m_graph.GetNode(m_hovered_node_id)->selected) {
                    m_graph.ClearSelection();
                }
            }
            m_graph.SelectNode(m_hovered_node_id, io.KeyCtrl || io.KeyShift);
            m_dragging_node = true;
            m_drag_start = mouse_pos;
        } else {
            // Start box selection
            if (!io.KeyCtrl && !io.KeyShift) {
                m_graph.ClearSelection();
            }
            m_box_selecting = true;
            m_selection_start = {io.MousePos.x, io.MousePos.y};
        }
    }
    
    // Dragging
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (m_dragging_link) {
            m_link_end_pos = mouse_pos;
        } else if (m_dragging_node) {
            vec2 delta = {io.MouseDelta.x / m_canvas_zoom, io.MouseDelta.y / m_canvas_zoom};
            for (auto& node : m_graph.GetNodes()) {
                if (node.selected) {
                    node.position = node.position + delta;
                    if (m_snap_to_grid) {
                        node.position.x = std::round(node.position.x / m_grid_size) * m_grid_size;
                        node.position.y = std::round(node.position.y / m_grid_size) * m_grid_size;
                    }
                }
            }
        } else if (m_box_selecting) {
            m_link_end_pos = {io.MousePos.x, io.MousePos.y};
        }
    }
    
    // Release
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_dragging_link) {
            // Try to create connection
            if (m_hovered_pin_id != 0 && m_hovered_pin_id != m_link_start_pin) {
                if (m_link_from_input) {
                    m_graph.AddLink(m_hovered_pin_id, m_link_start_pin);
                } else {
                    m_graph.AddLink(m_link_start_pin, m_hovered_pin_id);
                }
            }
            m_dragging_link = false;
            m_link_start_pin = 0;
        }
        if (m_box_selecting) {
            vec2 box_min = ScreenToCanvas({std::min(m_selection_start.x, m_link_end_pos.x),
                                           std::min(m_selection_start.y, m_link_end_pos.y)});
            vec2 box_max = ScreenToCanvas({std::max(m_selection_start.x, m_link_end_pos.x),
                                           std::max(m_selection_start.y, m_link_end_pos.y)});
            m_graph.SelectNodesInRect(box_min, box_max);
            m_box_selecting = false;
        }
        m_dragging_node = false;
    }
    
    // Right click for context menu
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_context_menu_pos = {io.MousePos.x, io.MousePos.y};
        if (m_hovered_node_id != 0) {
            if (!m_graph.GetNode(m_hovered_node_id)->selected) {
                m_graph.ClearSelection();
                m_graph.SelectNode(m_hovered_node_id);
            }
            m_show_node_context_menu = true;
        } else {
            m_show_context_menu = true;
        }
    }
    
    // Keyboard shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        auto selected = m_graph.GetSelectedNodeIds();
        for (u32 id : selected) {
            m_graph.DeleteNode(id);
        }
    }
    
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        for (auto& node : m_graph.GetNodes()) {
            node.selected = true;
        }
    }
}

vec2 ShaderGraphEditor::ScreenToCanvas(const vec2& screen_pos) const {
    return vec2{
        (screen_pos.x - m_canvas_screen_pos.x) / m_canvas_zoom - m_canvas_offset.x,
        (screen_pos.y - m_canvas_screen_pos.y) / m_canvas_zoom - m_canvas_offset.y
    };
}

vec2 ShaderGraphEditor::CanvasToScreen(const vec2& canvas_pos) const {
    return vec2{
        m_canvas_screen_pos.x + (canvas_pos.x + m_canvas_offset.x) * m_canvas_zoom,
        m_canvas_screen_pos.y + (canvas_pos.y + m_canvas_offset.y) * m_canvas_zoom
    };
}

vec2 ShaderGraphEditor::GetPinPosition(const ShaderNode& node, const ShaderPin& pin) const {
    float header_height = 24.0f;
    float pin_start_y = node.position.y + header_height + 8;
    float pin_spacing = 20.0f;
    
    if (pin.direction == ShaderPinDirection::Input) {
        for (size_t i = 0; i < node.inputs.size(); ++i) {
            if (node.inputs[i].id == pin.id) {
                return {node.position.x, pin_start_y + i * pin_spacing};
            }
        }
    } else {
        for (size_t i = 0; i < node.outputs.size(); ++i) {
            if (node.outputs[i].id == pin.id) {
                return {node.position.x + node.size.x, pin_start_y + i * pin_spacing};
            }
        }
    }
    
    return node.position;
}

ShaderNode* ShaderGraphEditor::HitTestNode(const vec2& canvas_pos) {
    // Test in reverse order (topmost first)
    for (auto it = m_graph.GetNodes().rbegin(); it != m_graph.GetNodes().rend(); ++it) {
        ShaderNode& node = *it;
        if (canvas_pos.x >= node.position.x && canvas_pos.x <= node.position.x + node.size.x &&
            canvas_pos.y >= node.position.y && canvas_pos.y <= node.position.y + node.size.y) {
            return &node;
        }
    }
    return nullptr;
}

ShaderPin* ShaderGraphEditor::HitTestPin(const vec2& canvas_pos, ShaderNode** out_node) {
    float hit_radius = 10.0f / m_canvas_zoom;
    
    auto vec2_length = [](const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); };
    
    for (auto& node : m_graph.GetNodes()) {
        for (auto& pin : node.inputs) {
            vec2 pin_pos = GetPinPosition(node, pin);
            vec2 diff = canvas_pos - pin_pos;
            float dist = vec2_length(diff);
            if (dist <= hit_radius) {
                if (out_node) *out_node = &node;
                return &pin;
            }
        }
        for (auto& pin : node.outputs) {
            vec2 pin_pos = GetPinPosition(node, pin);
            vec2 diff = canvas_pos - pin_pos;
            float dist = vec2_length(diff);
            if (dist <= hit_radius) {
                if (out_node) *out_node = &node;
                return &pin;
            }
        }
    }
    return nullptr;
}

} // namespace action
