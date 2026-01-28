#pragma once

#include "core/types.h"
#include "core/math/math.h"

namespace action {

class Renderer;

/*
 * GizmoOperation - What type of transform is being performed
 */
enum class GizmoOperation {
    None,
    Translate,
    Rotate,
    Scale
};

/*
 * GizmoAxis - Which axis is being manipulated
 */
enum class GizmoAxis {
    None,
    X,
    Y,
    Z,
    All     // Center/uniform
};

/*
 * GizmoPanel - Combined visual transform gizmo for 3D manipulation
 * 
 * Draws a unified gizmo with:
 * - Translation arrows (inner, near center)
 * - Rotation circles (outer ring)
 * - Scale boxes (at arrow tips)
 * 
 * Colors:
 * - X axis: Red
 * - Y axis: Green
 * - Z axis: Blue
 * - Rotation ring: Orange
 */
class GizmoPanel {
public:
    GizmoPanel() = default;
    ~GizmoPanel() = default;
    
    // Draw combined gizmo for a selected object
    // Returns true if gizmo was modified (transform changed)
    bool Draw(const Renderer& renderer, vec3& position, vec3& rotation, vec3& scale,
              const vec2& viewport_pos, const vec2& viewport_size);
    
    // Settings
    float gizmo_size = 80.0f;  // Base size in pixels
    bool enabled = true;
    
    // State
    bool IsManipulating() const { return m_active_op != GizmoOperation::None; }
    GizmoOperation GetActiveOperation() const { return m_active_op; }
    GizmoAxis GetActiveAxis() const { return m_active_axis; }
    
private:
    // Helper functions
    vec2 WorldToScreen(const vec3& world_pos, const mat4& view_proj, 
                       const vec2& viewport_pos, const vec2& viewport_size) const;
    
    // Draw the combined gizmo
    void DrawCombinedGizmo(const vec2& center);
    
    // Handle input - returns true if transform was modified
    bool HandleInput(vec3& position, vec3& rotation, vec3& scale,
                     const vec2& center, const mat4& view, const mat4& proj);
    
    // Hit test - determines what part of gizmo mouse is over
    void HitTest(const vec2& mouse_pos, const vec2& center,
                 GizmoOperation& out_op, GizmoAxis& out_axis);
    
    // State
    GizmoOperation m_active_op = GizmoOperation::None;
    GizmoAxis m_active_axis = GizmoAxis::None;
    GizmoOperation m_hovered_op = GizmoOperation::None;
    GizmoAxis m_hovered_axis = GizmoAxis::None;
    
    vec2 m_last_mouse{0, 0};
    bool m_is_dragging = false;
    
    // For undo/redo: store original values at drag start
    vec3 m_drag_start_position{0, 0, 0};
    vec3 m_drag_start_rotation{0, 0, 0};
    vec3 m_drag_start_scale{1, 1, 1};
    bool m_drag_made_changes = false;  // Track if anything actually changed
    
    // Cached axis directions in screen space
    vec2 m_axis_screen_dirs[3];  // X, Y, Z normalized directions
    vec2 m_axis_screen_ends[3];  // Endpoints for translation arrows
    vec2 m_scale_box_pos[3];     // Scale box positions
    
public:
    // Get original values at drag start (for command creation)
    const vec3& GetDragStartPosition() const { return m_drag_start_position; }
    const vec3& GetDragStartRotation() const { return m_drag_start_rotation; }
    const vec3& GetDragStartScale() const { return m_drag_start_scale; }
    bool DragMadeChanges() const { return m_drag_made_changes; }
    bool DragJustEnded() const { return m_drag_just_ended; }
    
private:
    bool m_drag_just_ended = false;
};

} // namespace action
