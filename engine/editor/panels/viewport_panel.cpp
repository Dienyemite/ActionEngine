#include "viewport_panel.h"
#include "render/renderer.h"
#include <imgui/imgui.h>

namespace action {

void ViewportPanel::Draw(Renderer& renderer, bool play_mode) {
    if (!visible) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (play_mode) {
        flags |= ImGuiWindowFlags_NoInputs;  // Don't steal input during play
    }
    
    if (ImGui::Begin("Viewport", &visible, flags)) {
        m_focused = ImGui::IsWindowFocused();
        m_hovered = ImGui::IsWindowHovered();
        
        // Draw toolbar at top of viewport
        DrawToolbar(renderer);
        
        ImGui::Separator();
        
        // Draw viewport content
        DrawViewportContent(renderer, play_mode);
    }
    ImGui::End();
    
    ImGui::PopStyleVar();
}

void ViewportPanel::DrawToolbar(Renderer& renderer) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    
    // View mode selector
    const char* view_modes[] = { "Perspective", "Top", "Front", "Right" };
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("##ViewMode", &view_mode, view_modes, IM_ARRAYSIZE(view_modes));
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    // Toggle buttons
    ImGui::Checkbox("Grid", &show_grid);
    ImGui::SameLine();
    ImGui::Checkbox("Gizmos", &show_gizmos);
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    // Camera info
    const Camera& cam = renderer.GetCamera();
    ImGui::Text("Pos: (%.1f, %.1f, %.1f)", cam.position.x, cam.position.y, cam.position.z);
    
    ImGui::PopStyleVar(2);
}

void ViewportPanel::DrawViewportContent(Renderer& renderer, bool play_mode) {
    // Get available region size
    ImVec2 region = ImGui::GetContentRegionAvail();
    m_viewport_size = {region.x, region.y};
    
    // In the future, we'll render to an offscreen texture and display it here
    // For now, show a placeholder with info
    
    ImVec2 pos = ImGui::GetCursorScreenPos();
    m_viewport_screen_pos = {pos.x, pos.y};  // Store for gizmo rendering
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Draw dark background
    draw_list->AddRectFilled(
        pos, 
        ImVec2(pos.x + region.x, pos.y + region.y),
        IM_COL32(30, 30, 35, 255)
    );
    
    // Draw grid pattern (visual indicator)
    if (show_grid) {
        float grid_size = 50.0f;
        ImU32 grid_color = IM_COL32(50, 50, 55, 255);
        
        for (float x = 0; x < region.x; x += grid_size) {
            draw_list->AddLine(
                ImVec2(pos.x + x, pos.y),
                ImVec2(pos.x + x, pos.y + region.y),
                grid_color
            );
        }
        for (float y = 0; y < region.y; y += grid_size) {
            draw_list->AddLine(
                ImVec2(pos.x, pos.y + y),
                ImVec2(pos.x + region.x, pos.y + y),
                grid_color
            );
        }
    }
    
    // Center message
    const char* message = play_mode ? "PLAYING" : "3D Viewport";
    ImVec2 text_size = ImGui::CalcTextSize(message);
    ImVec2 text_pos = ImVec2(
        pos.x + (region.x - text_size.x) * 0.5f,
        pos.y + (region.y - text_size.y) * 0.5f
    );
    
    if (play_mode) {
        draw_list->AddText(text_pos, IM_COL32(100, 200, 100, 255), message);
    } else {
        draw_list->AddText(text_pos, IM_COL32(150, 150, 150, 255), message);
    }
    
    // Show info at bottom
    char info[128];
    snprintf(info, sizeof(info), "%.0f x %.0f", region.x, region.y);
    ImVec2 info_pos = ImVec2(pos.x + 8, pos.y + region.y - 20);
    draw_list->AddText(info_pos, IM_COL32(100, 100, 100, 255), info);
    
    // Status indicators
    if (m_focused) {
        draw_list->AddText(ImVec2(pos.x + 8, pos.y + 8), IM_COL32(100, 200, 100, 255), "Focused");
    }
    
    // Make the region interactive
    ImGui::InvisibleButton("viewport_area", region);
}

} // namespace action
