#include "multi_viewport_panel.h"
#include "render/renderer.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <imgui/imgui.h>
#include <cmath>

namespace action {

mat4 SingleViewport::GetViewMatrix() const {
    // For orthographic modes, use a fixed camera orientation
    vec3 eye = camera_position;
    vec3 target = camera_target;
    vec3 up{0, 1, 0};
    
    switch (mode) {
        case ViewportMode::Top:
            eye = camera_target + vec3{0, camera_distance, 0};
            up = {0, 0, -1};
            break;
        case ViewportMode::Bottom:
            eye = camera_target + vec3{0, -camera_distance, 0};
            up = {0, 0, 1};
            break;
        case ViewportMode::Front:
            eye = camera_target + vec3{0, 0, camera_distance};
            break;
        case ViewportMode::Back:
            eye = camera_target + vec3{0, 0, -camera_distance};
            break;
        case ViewportMode::Left:
            eye = camera_target + vec3{-camera_distance, 0, 0};
            break;
        case ViewportMode::Right:
            eye = camera_target + vec3{camera_distance, 0, 0};
            break;
        case ViewportMode::Perspective:
        case ViewportMode::Custom:
        default:
            // Use orbit camera position
            float yaw_rad = camera_yaw * DEG_TO_RAD;
            float pitch_rad = camera_pitch * DEG_TO_RAD;
            
            float cos_pitch = std::cos(pitch_rad);
            float sin_pitch = std::sin(pitch_rad);
            float cos_yaw = std::cos(yaw_rad);
            float sin_yaw = std::sin(yaw_rad);
            
            eye = camera_target + vec3{
                camera_distance * cos_pitch * sin_yaw,
                camera_distance * sin_pitch,
                camera_distance * cos_pitch * cos_yaw
            };
            break;
    }
    
    // Use the existing look_at function
    return mat4::look_at(eye, target, up);
}

mat4 SingleViewport::GetProjectionMatrix(float aspect) const {
    if (mode == ViewportMode::Perspective || mode == ViewportMode::Custom) {
        // Use the existing perspective function
        return mat4::perspective(fov * DEG_TO_RAD, aspect, 0.1f, 1000.0f);
    } else {
        // Orthographic projection for side views
        float half_size = camera_distance * camera_zoom;
        float half_width = half_size * aspect;
        float half_height = half_size;
        float near_plane = 0.1f;
        float far_plane = 1000.0f;
        
        mat4 proj = mat4::identity();
        proj.m[0][0] = 1.0f / half_width;
        proj.m[1][1] = 1.0f / half_height;
        proj.m[2][2] = -2.0f / (far_plane - near_plane);
        proj.m[3][2] = -(far_plane + near_plane) / (far_plane - near_plane);
        
        return proj;
    }
}

void SingleViewport::SetOrthographicMode(ViewportMode new_mode) {
    mode = new_mode;
    UpdateCamera();
}

void SingleViewport::UpdateCamera() {
    // Set default camera positions based on mode
    switch (mode) {
        case ViewportMode::Top:
            name = "Top";
            break;
        case ViewportMode::Bottom:
            name = "Bottom";
            break;
        case ViewportMode::Front:
            name = "Front";
            break;
        case ViewportMode::Back:
            name = "Back";
            break;
        case ViewportMode::Left:
            name = "Left";
            break;
        case ViewportMode::Right:
            name = "Right";
            break;
        case ViewportMode::Perspective:
            name = "Perspective";
            break;
        case ViewportMode::Custom:
            name = "Custom";
            break;
    }
}

MultiViewportPanel::MultiViewportPanel() {
    Initialize();
}

void MultiViewportPanel::Initialize() {
    // Create default single viewport
    SingleViewport vp;
    vp.id = m_next_viewport_id++;
    vp.name = "Perspective";
    vp.mode = ViewportMode::Perspective;
    vp.camera_distance = 10.0f;
    vp.camera_yaw = 45.0f;
    vp.camera_pitch = -30.0f;
    m_viewports.push_back(vp);
    
    m_active_viewport_id = vp.id;
}

void MultiViewportPanel::SetLayout(ViewportLayout layout) {
    if (m_layout == layout) return;
    
    m_layout = layout;
    
    // Adjust viewport count based on layout
    size_t needed = 1;
    switch (layout) {
        case ViewportLayout::Single: needed = 1; break;
        case ViewportLayout::TwoHorizontal:
        case ViewportLayout::TwoVertical: needed = 2; break;
        case ViewportLayout::ThreeLeft:
        case ViewportLayout::ThreeRight:
        case ViewportLayout::ThreeTop:
        case ViewportLayout::ThreeBottom: needed = 3; break;
        case ViewportLayout::Quad: needed = 4; break;
        default: needed = 1; break;
    }
    
    // Add viewports if needed
    while (m_viewports.size() < needed) {
        SingleViewport vp;
        vp.id = m_next_viewport_id++;
        
        // Set default modes for new viewports
        switch (m_viewports.size()) {
            case 1:
                vp.mode = ViewportMode::Top;
                vp.name = "Top";
                break;
            case 2:
                vp.mode = ViewportMode::Front;
                vp.name = "Front";
                break;
            case 3:
                vp.mode = ViewportMode::Right;
                vp.name = "Right";
                break;
            default:
                vp.mode = ViewportMode::Perspective;
                vp.name = "Viewport";
        }
        vp.UpdateCamera();
        m_viewports.push_back(vp);
    }
    
    // Remove excess viewports
    while (m_viewports.size() > needed) {
        m_viewports.pop_back();
    }
    
    // Make sure active viewport is valid
    bool found = false;
    for (const auto& vp : m_viewports) {
        if (vp.id == m_active_viewport_id) {
            found = true;
            break;
        }
    }
    if (!found && !m_viewports.empty()) {
        m_active_viewport_id = m_viewports[0].id;
    }
    
    UpdateViewportBounds();
}

void MultiViewportPanel::UpdateViewportBounds() {
    if (m_viewports.empty()) return;
    
    float w = m_panel_size.x;
    float h = m_panel_size.y - 30.0f;  // Leave room for toolbar
    float padding = 2.0f;
    
    switch (m_layout) {
        case ViewportLayout::Single:
            m_viewports[0].position = {0, 0};
            m_viewports[0].size = {w, h};
            break;
            
        case ViewportLayout::TwoHorizontal:
            m_viewports[0].position = {0, 0};
            m_viewports[0].size = {w * 0.5f - padding, h};
            m_viewports[1].position = {w * 0.5f + padding, 0};
            m_viewports[1].size = {w * 0.5f - padding, h};
            break;
            
        case ViewportLayout::TwoVertical:
            m_viewports[0].position = {0, 0};
            m_viewports[0].size = {w, h * 0.5f - padding};
            m_viewports[1].position = {0, h * 0.5f + padding};
            m_viewports[1].size = {w, h * 0.5f - padding};
            break;
            
        case ViewportLayout::ThreeLeft:
            m_viewports[0].position = {0, 0};
            m_viewports[0].size = {w * 0.6f - padding, h};
            m_viewports[1].position = {w * 0.6f + padding, 0};
            m_viewports[1].size = {w * 0.4f - padding, h * 0.5f - padding};
            m_viewports[2].position = {w * 0.6f + padding, h * 0.5f + padding};
            m_viewports[2].size = {w * 0.4f - padding, h * 0.5f - padding};
            break;
            
        case ViewportLayout::ThreeRight:
            m_viewports[0].position = {w * 0.4f + padding, 0};
            m_viewports[0].size = {w * 0.6f - padding, h};
            m_viewports[1].position = {0, 0};
            m_viewports[1].size = {w * 0.4f - padding, h * 0.5f - padding};
            m_viewports[2].position = {0, h * 0.5f + padding};
            m_viewports[2].size = {w * 0.4f - padding, h * 0.5f - padding};
            break;
            
        case ViewportLayout::ThreeTop:
            m_viewports[0].position = {0, 0};
            m_viewports[0].size = {w, h * 0.6f - padding};
            m_viewports[1].position = {0, h * 0.6f + padding};
            m_viewports[1].size = {w * 0.5f - padding, h * 0.4f - padding};
            m_viewports[2].position = {w * 0.5f + padding, h * 0.6f + padding};
            m_viewports[2].size = {w * 0.5f - padding, h * 0.4f - padding};
            break;
            
        case ViewportLayout::ThreeBottom:
            m_viewports[0].position = {0, h * 0.4f + padding};
            m_viewports[0].size = {w, h * 0.6f - padding};
            m_viewports[1].position = {0, 0};
            m_viewports[1].size = {w * 0.5f - padding, h * 0.4f - padding};
            m_viewports[2].position = {w * 0.5f + padding, 0};
            m_viewports[2].size = {w * 0.5f - padding, h * 0.4f - padding};
            break;
            
        case ViewportLayout::Quad:
            m_viewports[0].position = {0, 0};
            m_viewports[0].size = {w * 0.5f - padding, h * 0.5f - padding};
            m_viewports[1].position = {w * 0.5f + padding, 0};
            m_viewports[1].size = {w * 0.5f - padding, h * 0.5f - padding};
            m_viewports[2].position = {0, h * 0.5f + padding};
            m_viewports[2].size = {w * 0.5f - padding, h * 0.5f - padding};
            m_viewports[3].position = {w * 0.5f + padding, h * 0.5f + padding};
            m_viewports[3].size = {w * 0.5f - padding, h * 0.5f - padding};
            break;
            
        default:
            break;
    }
}

void MultiViewportPanel::Draw(Renderer& renderer, bool play_mode) {
    if (!visible) return;
    
    // Reset per-frame state
    m_left_clicked_this_frame = false;
    m_any_viewport_hovered = false;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (play_mode) {
        flags |= ImGuiWindowFlags_NoInputs;
    }
    
    if (ImGui::Begin("Viewports", &visible, flags)) {
        // Store panel position and size
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        m_panel_position = {pos.x, pos.y};
        m_panel_size = {size.x, size.y};
        
        // Draw layout selector in toolbar
        DrawLayoutSelector();
        
        ImGui::Separator();
        
        // Update viewport bounds based on panel size
        UpdateViewportBounds();
        
        // Draw each viewport
        ImVec2 base_pos = ImGui::GetCursorScreenPos();
        for (auto& viewport : m_viewports) {
            DrawViewport(viewport, renderer, play_mode);
        }
    }
    ImGui::End();
    
    ImGui::PopStyleVar();
}

void MultiViewportPanel::DrawLayoutSelector() {
    ImGui::Text("Layout:");
    ImGui::SameLine();
    
    const char* layouts[] = { "Single", "2-H", "2-V", "3-L", "3-R", "3-T", "3-B", "Quad" };
    int current = static_cast<int>(m_layout);
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("##Layout", &current, layouts, IM_ARRAYSIZE(layouts))) {
        SetLayout(static_cast<ViewportLayout>(current));
    }
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    ImGui::Checkbox("Sync Cameras", &sync_cameras);
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    // Show active viewport info
    if (SingleViewport* active = GetActiveViewport()) {
        ImGui::Text("Active: %s", active->name.c_str());
    }
}

void MultiViewportPanel::DrawViewport(SingleViewport& viewport, Renderer& renderer, bool play_mode) {
    ImVec2 base_pos = ImGui::GetCursorScreenPos();
    ImVec2 vp_pos(base_pos.x + viewport.position.x, base_pos.y + viewport.position.y);
    ImVec2 vp_size(viewport.size.x, viewport.size.y);
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Viewport background
    ImU32 bg_color = (viewport.id == m_active_viewport_id) 
                     ? IM_COL32(35, 35, 40, 255) 
                     : IM_COL32(25, 25, 30, 255);
    draw_list->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), bg_color);
    
    // Border
    ImU32 border_color = (viewport.id == m_active_viewport_id)
                         ? IM_COL32(100, 150, 200, 255)
                         : IM_COL32(60, 60, 65, 255);
    draw_list->AddRect(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), border_color);
    
    // Viewport name label
    draw_list->AddText(ImVec2(vp_pos.x + 8, vp_pos.y + 4), IM_COL32(200, 200, 200, 255), viewport.name.c_str());
    
    // Mode indicator
    const char* mode_str = "?";
    switch (viewport.mode) {
        case ViewportMode::Perspective: mode_str = "Persp"; break;
        case ViewportMode::Top: mode_str = "Top"; break;
        case ViewportMode::Bottom: mode_str = "Bottom"; break;
        case ViewportMode::Front: mode_str = "Front"; break;
        case ViewportMode::Back: mode_str = "Back"; break;
        case ViewportMode::Left: mode_str = "Left"; break;
        case ViewportMode::Right: mode_str = "Right"; break;
        case ViewportMode::Custom: mode_str = "Custom"; break;
    }
    
    ImVec2 mode_text_size = ImGui::CalcTextSize(mode_str);
    draw_list->AddText(
        ImVec2(vp_pos.x + vp_size.x - mode_text_size.x - 8, vp_pos.y + 4),
        IM_COL32(150, 150, 150, 255), mode_str
    );
    
    // Grid for orthographic views
    if (viewport.show_grid && viewport.mode != ViewportMode::Perspective) {
        float grid_size = 40.0f;
        ImU32 grid_color = IM_COL32(45, 45, 50, 255);
        
        float content_y = vp_pos.y + 20;
        float content_h = vp_size.y - 20;
        
        for (float x = 0; x < vp_size.x; x += grid_size) {
            draw_list->AddLine(
                ImVec2(vp_pos.x + x, content_y),
                ImVec2(vp_pos.x + x, vp_pos.y + vp_size.y),
                grid_color
            );
        }
        for (float y = 0; y < content_h; y += grid_size) {
            draw_list->AddLine(
                ImVec2(vp_pos.x, content_y + y),
                ImVec2(vp_pos.x + vp_size.x, content_y + y),
                grid_color
            );
        }
        
        // Center crosshair
        float cx = vp_pos.x + vp_size.x * 0.5f;
        float cy = content_y + content_h * 0.5f;
        draw_list->AddLine(ImVec2(cx - 15, cy), ImVec2(cx + 15, cy), IM_COL32(100, 100, 100, 255));
        draw_list->AddLine(ImVec2(cx, cy - 15), ImVec2(cx, cy + 15), IM_COL32(100, 100, 100, 255));
    }
    
    // Axis indicator for perspective view
    if (viewport.mode == ViewportMode::Perspective) {
        float axis_x = vp_pos.x + 30;
        float axis_y = vp_pos.y + vp_size.y - 30;
        float axis_len = 20.0f;
        
        // Simple axis indicator (X=red, Y=green, Z=blue)
        draw_list->AddLine(ImVec2(axis_x, axis_y), ImVec2(axis_x + axis_len, axis_y), IM_COL32(200, 60, 60, 255), 2.0f);
        draw_list->AddLine(ImVec2(axis_x, axis_y), ImVec2(axis_x, axis_y - axis_len), IM_COL32(60, 200, 60, 255), 2.0f);
        draw_list->AddLine(ImVec2(axis_x, axis_y), ImVec2(axis_x - axis_len * 0.5f, axis_y + axis_len * 0.5f), IM_COL32(60, 60, 200, 255), 2.0f);
        
        draw_list->AddText(ImVec2(axis_x + axis_len + 2, axis_y - 6), IM_COL32(200, 60, 60, 255), "X");
        draw_list->AddText(ImVec2(axis_x - 6, axis_y - axis_len - 12), IM_COL32(60, 200, 60, 255), "Y");
        draw_list->AddText(ImVec2(axis_x - axis_len * 0.5f - 12, axis_y + axis_len * 0.5f - 6), IM_COL32(60, 60, 200, 255), "Z");
    }
    
    // Camera info for perspective
    if (viewport.mode == ViewportMode::Perspective) {
        char info[64];
        snprintf(info, sizeof(info), "D:%.1f Y:%.0f° P:%.0f°", 
                 viewport.camera_distance, viewport.camera_yaw, viewport.camera_pitch);
        ImVec2 info_size = ImGui::CalcTextSize(info);
        draw_list->AddText(
            ImVec2(vp_pos.x + 8, vp_pos.y + vp_size.y - 18),
            IM_COL32(100, 100, 100, 255), info
        );
    }
    
    // Size info
    char size_info[32];
    snprintf(size_info, sizeof(size_info), "%.0fx%.0f", viewport.size.x, viewport.size.y);
    ImVec2 size_info_size = ImGui::CalcTextSize(size_info);
    draw_list->AddText(
        ImVec2(vp_pos.x + vp_size.x - size_info_size.x - 8, vp_pos.y + vp_size.y - 18),
        IM_COL32(80, 80, 80, 255), size_info
    );
    
    // Handle input for this viewport
    ImGui::SetCursorScreenPos(vp_pos);
    ImGui::InvisibleButton(("viewport_" + std::to_string(viewport.id)).c_str(), vp_size);
    
    viewport.hovered = ImGui::IsItemHovered();
    if (viewport.hovered) {
        m_any_viewport_hovered = true;
    }
    if (ImGui::IsItemClicked()) {
        m_active_viewport_id = viewport.id;
    }
    
    // Detect left-click for object picking (actual window coordinates)
    // Note: Picking is now handled by Engine::Update via TryPickAtScreenPosition
    
    // Camera control for perspective viewports
    if (viewport.hovered && viewport.mode == ViewportMode::Perspective) {
        HandleViewportInput(viewport);
    }
}

void MultiViewportPanel::HandleViewportInput(SingleViewport& viewport) {
    ImGuiIO& io = ImGui::GetIO();
    
    // Middle mouse button for orbiting
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        float dx = io.MouseDelta.x;
        float dy = io.MouseDelta.y;
        
        if (io.KeyShift) {
            // Pan
            // TODO: Implement proper panning based on camera orientation
        } else {
            // Orbit
            viewport.camera_yaw -= dx * 0.3f;
            viewport.camera_pitch -= dy * 0.3f;
            
            // Clamp pitch
            if (viewport.camera_pitch > 89.0f) viewport.camera_pitch = 89.0f;
            if (viewport.camera_pitch < -89.0f) viewport.camera_pitch = -89.0f;
        }
    }
    
    // Scroll wheel for zoom
    if (io.MouseWheel != 0.0f) {
        viewport.camera_distance -= io.MouseWheel * viewport.camera_distance * 0.1f;
        if (viewport.camera_distance < 0.5f) viewport.camera_distance = 0.5f;
        if (viewport.camera_distance > 500.0f) viewport.camera_distance = 500.0f;
    }
}

SingleViewport* MultiViewportPanel::GetActiveViewport() {
    for (auto& vp : m_viewports) {
        if (vp.id == m_active_viewport_id) {
            return &vp;
        }
    }
    return m_viewports.empty() ? nullptr : &m_viewports[0];
}

const SingleViewport* MultiViewportPanel::GetActiveViewport() const {
    for (const auto& vp : m_viewports) {
        if (vp.id == m_active_viewport_id) {
            return &vp;
        }
    }
    return m_viewports.empty() ? nullptr : &m_viewports[0];
}

SingleViewport* MultiViewportPanel::GetViewport(u32 id) {
    for (auto& vp : m_viewports) {
        if (vp.id == id) {
            return &vp;
        }
    }
    return nullptr;
}

void MultiViewportPanel::SetActiveViewport(u32 id) {
    if (GetViewport(id)) {
        m_active_viewport_id = id;
    }
}

} // namespace action
