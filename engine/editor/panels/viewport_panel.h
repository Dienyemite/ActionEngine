#pragma once

#include "core/types.h"

namespace action {

class Renderer;

/*
 * ViewportPanel - 3D scene viewport
 * 
 * Displays the 3D scene with:
 * - Orbit/fly camera controls
 * - Gizmo overlays (future)
 * - Grid visualization
 */

class ViewportPanel {
public:
    ViewportPanel() = default;
    ~ViewportPanel() = default;
    
    void Draw(Renderer& renderer, bool play_mode);
    
    bool visible = true;
    
    // Viewport settings
    bool show_grid = true;
    bool show_gizmos = true;
    int view_mode = 0;  // 0=Perspective, 1=Top, 2=Front, 3=Right
    
    // Get viewport info (for gizmo rendering)
    vec2 GetViewportScreenPos() const { return m_viewport_screen_pos; }
    vec2 GetViewportSize() const { return m_viewport_size; }
    
private:
    void DrawToolbar(Renderer& renderer);
    void DrawViewportContent(Renderer& renderer, bool play_mode);
    
    bool m_focused = false;
    bool m_hovered = false;
    
    vec2 m_viewport_size{800, 600};
    vec2 m_viewport_screen_pos{0, 0};
};

} // namespace action
