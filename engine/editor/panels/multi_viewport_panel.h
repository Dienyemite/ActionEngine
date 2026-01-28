#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <memory>

namespace action {

class Renderer;

/*
 * ViewportConfig - Configuration for a single viewport
 */
enum class ViewportMode {
    Perspective = 0,
    Top,
    Bottom,
    Front,
    Back,
    Left,
    Right,
    Custom
};

enum class ViewportLayout {
    Single = 0,      // One viewport
    TwoHorizontal,   // Two viewports side by side
    TwoVertical,     // Two viewports stacked
    ThreeLeft,       // One big left, two small right
    ThreeRight,      // One big right, two small left
    ThreeTop,        // One big top, two small bottom
    ThreeBottom,     // One big bottom, two small top
    Quad,            // 2x2 grid
    Count
};

/*
 * SingleViewport - Represents one viewport within the multi-viewport system
 */
struct SingleViewport {
    u32 id = 0;
    std::string name = "Viewport";
    ViewportMode mode = ViewportMode::Perspective;
    
    // Camera for this viewport (orthographic views use fixed cameras)
    vec3 camera_position{0, 5, 10};
    vec3 camera_target{0, 0, 0};
    float camera_distance = 10.0f;  // For orbit control
    float camera_yaw = 0.0f;        // Horizontal angle
    float camera_pitch = -15.0f;    // Vertical angle
    float camera_zoom = 1.0f;       // Zoom level (for orthographic)
    float fov = 60.0f;
    
    // Viewport bounds (in screen space within the panel)
    vec2 position{0, 0};
    vec2 size{400, 300};
    
    // State
    bool focused = false;
    bool hovered = false;
    bool is_active = false;  // Currently the "active" viewport for input
    
    // Display options
    bool show_grid = true;
    bool show_gizmos = true;
    bool wireframe = false;
    bool show_stats = false;
    
    // Get camera matrix for this viewport
    mat4 GetViewMatrix() const;
    mat4 GetProjectionMatrix(float aspect) const;
    
    // Update camera for this viewport mode
    void SetOrthographicMode(ViewportMode new_mode);
    void UpdateCamera();
};

/*
 * MultiViewportPanel - Manages multiple 3D viewports
 * 
 * Features:
 * - Configurable layouts (single, split, quad)
 * - Different view modes per viewport
 * - Independent camera controls
 * - Active viewport selection
 */
class MultiViewportPanel {
public:
    MultiViewportPanel();
    ~MultiViewportPanel() = default;
    
    void Initialize();
    void Draw(Renderer& renderer, bool play_mode);
    
    // Layout management
    void SetLayout(ViewportLayout layout);
    ViewportLayout GetLayout() const { return m_layout; }
    
    // Viewport access
    SingleViewport* GetActiveViewport();
    const SingleViewport* GetActiveViewport() const;
    SingleViewport* GetViewport(u32 id);
    std::vector<SingleViewport>& GetViewports() { return m_viewports; }
    
    // Set active viewport
    void SetActiveViewport(u32 id);
    
    // Panel visibility
    bool visible = true;
    
    // Overall panel settings
    bool sync_cameras = false;  // Keep all perspective cameras in sync
    
private:
    void DrawLayoutSelector();
    void DrawViewport(SingleViewport& viewport, Renderer& renderer, bool play_mode);
    void DrawViewportContent(SingleViewport& viewport, Renderer& renderer, bool play_mode);
    void DrawViewportToolbar(SingleViewport& viewport);
    void UpdateViewportBounds();
    void HandleViewportInput(SingleViewport& viewport);
    
    ViewportLayout m_layout = ViewportLayout::Single;
    std::vector<SingleViewport> m_viewports;
    u32 m_active_viewport_id = 1;
    u32 m_next_viewport_id = 1;
    
    vec2 m_panel_position{0, 0};
    vec2 m_panel_size{800, 600};
    
    // Mouse state for camera control
    bool m_dragging = false;
    vec2 m_last_mouse_pos{0, 0};
    bool m_is_orbiting = false;
    bool m_is_panning = false;
};

} // namespace action
