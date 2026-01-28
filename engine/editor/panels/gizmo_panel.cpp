#include "gizmo_panel.h"
#include "render/renderer.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <cmath>

namespace action {

// Gizmo colors
static const ImU32 COLOR_X = IM_COL32(230, 60, 60, 255);       // Red
static const ImU32 COLOR_Y = IM_COL32(60, 200, 60, 255);       // Green  
static const ImU32 COLOR_Z = IM_COL32(60, 100, 230, 255);      // Blue
static const ImU32 COLOR_X_HOVER = IM_COL32(255, 120, 120, 255);
static const ImU32 COLOR_Y_HOVER = IM_COL32(120, 255, 120, 255);
static const ImU32 COLOR_Z_HOVER = IM_COL32(120, 160, 255, 255);
static const ImU32 COLOR_RING = IM_COL32(255, 160, 50, 255);   // Orange rotation ring
static const ImU32 COLOR_RING_HOVER = IM_COL32(255, 200, 100, 255);
static const ImU32 COLOR_CENTER = IM_COL32(255, 255, 255, 220);
static const ImU32 COLOR_CENTER_HOVER = IM_COL32(255, 255, 150, 255);

// Sizes
static constexpr float ARROW_LENGTH = 70.0f;
static constexpr float ARROW_HEAD_SIZE = 10.0f;
static constexpr float ARROW_THICKNESS = 2.5f;
static constexpr float ROTATION_RING_RADIUS = 85.0f;
static constexpr float ROTATION_RING_THICKNESS = 3.0f;
static constexpr float SCALE_BOX_SIZE = 6.0f;
static constexpr float CENTER_SIZE = 8.0f;
static constexpr float HIT_TOLERANCE = 8.0f;

vec2 GizmoPanel::WorldToScreen(const vec3& world_pos, const mat4& view_proj,
                                const vec2& viewport_pos, const vec2& viewport_size) const {
    vec4 clip = view_proj * vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);
    
    if (std::abs(clip.w) < 0.0001f) {
        return vec2(-10000, -10000);
    }
    
    vec3 ndc = vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
    
    // Vulkan's projection matrix already flips Y (NDC Y goes down like screen coords)
    // So we don't need to flip Y again here
    vec2 screen;
    screen.x = viewport_pos.x + (ndc.x * 0.5f + 0.5f) * viewport_size.x;
    screen.y = viewport_pos.y + (ndc.y * 0.5f + 0.5f) * viewport_size.y;
    
    return screen;
}

bool GizmoPanel::Draw(const Renderer& renderer, vec3& position, vec3& rotation, vec3& scale,
                      const vec2& viewport_pos, const vec2& viewport_size) {
    if (!enabled) return false;
    
    const Camera& camera = renderer.GetCamera();
    mat4 view = camera.GetViewMatrix();
    mat4 proj = camera.GetProjectionMatrix();
    mat4 view_proj = proj * view;
    
    // Always recalculate center from current position (follows object in real-time)
    vec2 center = WorldToScreen(position, view_proj, viewport_pos, viewport_size);
    
    // Check if on screen
    if (center.x < -200 || center.x > viewport_size.x + 200 ||
        center.y < -200 || center.y > viewport_size.y + 200) {
        return false;
    }
    
    // Calculate axis directions in screen space from current position
    vec2 x_end = WorldToScreen(position + vec3(1.0f, 0, 0), view_proj, viewport_pos, viewport_size);
    vec2 y_end = WorldToScreen(position + vec3(0, 1.0f, 0), view_proj, viewport_pos, viewport_size);
    vec2 z_end = WorldToScreen(position + vec3(0, 0, 1.0f), view_proj, viewport_pos, viewport_size);
    
    // Normalized directions
    vec2 x_dir = vec2(x_end.x - center.x, x_end.y - center.y);
    vec2 y_dir = vec2(y_end.x - center.x, y_end.y - center.y);
    vec2 z_dir = vec2(z_end.x - center.x, z_end.y - center.y);
    
    auto normalize2 = [](vec2& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y);
        if (len > 0.01f) { v.x /= len; v.y /= len; }
    };
    
    normalize2(x_dir);
    normalize2(y_dir);
    normalize2(z_dir);
    
    m_axis_screen_dirs[0] = x_dir;
    m_axis_screen_dirs[1] = y_dir;
    m_axis_screen_dirs[2] = z_dir;
    
    // Calculate arrow endpoints
    m_axis_screen_ends[0] = vec2(center.x + x_dir.x * ARROW_LENGTH, center.y + x_dir.y * ARROW_LENGTH);
    m_axis_screen_ends[1] = vec2(center.x + y_dir.x * ARROW_LENGTH, center.y + y_dir.y * ARROW_LENGTH);
    m_axis_screen_ends[2] = vec2(center.x + z_dir.x * ARROW_LENGTH, center.y + z_dir.y * ARROW_LENGTH);
    
    // Scale boxes are at arrow tips
    m_scale_box_pos[0] = m_axis_screen_ends[0];
    m_scale_box_pos[1] = m_axis_screen_ends[1];
    m_scale_box_pos[2] = m_axis_screen_ends[2];
    
    // Draw the combined gizmo
    DrawCombinedGizmo(center);
    
    // Handle input
    return HandleInput(position, rotation, scale, center, view, proj);
}

void GizmoPanel::DrawCombinedGizmo(const vec2& center) {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    
    // Determine colors based on hover/active state
    auto getColor = [this](int axis, GizmoOperation op) -> ImU32 {
        bool active = (m_active_op == op && m_active_axis == static_cast<GizmoAxis>(axis + 1));
        bool hovered = (m_hovered_op == op && m_hovered_axis == static_cast<GizmoAxis>(axis + 1));
        
        if (active || hovered) {
            switch (axis) {
                case 0: return COLOR_X_HOVER;
                case 1: return COLOR_Y_HOVER;
                case 2: return COLOR_Z_HOVER;
            }
        }
        switch (axis) {
            case 0: return COLOR_X;
            case 1: return COLOR_Y;
            case 2: return COLOR_Z;
        }
        return COLOR_CENTER;
    };
    
    // 1. Draw rotation ring (outer circle) - orange
    bool ring_active = (m_active_op == GizmoOperation::Rotate);
    bool ring_hovered = (m_hovered_op == GizmoOperation::Rotate);
    ImU32 ring_color = (ring_active || ring_hovered) ? COLOR_RING_HOVER : COLOR_RING;
    draw_list->AddCircle(ImVec2(center.x, center.y), ROTATION_RING_RADIUS, ring_color, 48, ROTATION_RING_THICKNESS);
    
    // Draw small rotation axis indicators on the ring
    for (int i = 0; i < 3; i++) {
        float angle_offset = (float)i * 2.094f;  // 120 degrees apart
        vec2 ring_pos(center.x + std::cos(angle_offset) * ROTATION_RING_RADIUS,
                      center.y + std::sin(angle_offset) * ROTATION_RING_RADIUS);
        ImU32 col = getColor(i, GizmoOperation::Rotate);
        draw_list->AddCircleFilled(ImVec2(ring_pos.x, ring_pos.y), 5.0f, col);
    }
    
    // 2. Draw translation arrows (from center outward)
    const char* axis_labels[] = {"X", "Y", "Z"};
    
    for (int i = 0; i < 3; i++) {
        ImU32 color = getColor(i, GizmoOperation::Translate);
        vec2 start = center;
        vec2 end = m_axis_screen_ends[i];
        vec2 dir = m_axis_screen_dirs[i];
        
        // Draw arrow line
        draw_list->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), color, ARROW_THICKNESS);
        
        // Draw arrow head
        vec2 perp(-dir.y, dir.x);
        vec2 arrow_base(end.x - dir.x * ARROW_HEAD_SIZE, end.y - dir.y * ARROW_HEAD_SIZE);
        draw_list->AddTriangleFilled(
            ImVec2(end.x, end.y),
            ImVec2(arrow_base.x + perp.x * ARROW_HEAD_SIZE * 0.4f, arrow_base.y + perp.y * ARROW_HEAD_SIZE * 0.4f),
            ImVec2(arrow_base.x - perp.x * ARROW_HEAD_SIZE * 0.4f, arrow_base.y - perp.y * ARROW_HEAD_SIZE * 0.4f),
            color
        );
    }
    
    // 3. Draw scale boxes at arrow tips
    for (int i = 0; i < 3; i++) {
        ImU32 color = getColor(i, GizmoOperation::Scale);
        vec2 pos = m_scale_box_pos[i];
        
        // Offset slightly beyond arrow tip
        vec2 dir = m_axis_screen_dirs[i];
        pos.x += dir.x * 8.0f;
        pos.y += dir.y * 8.0f;
        
        draw_list->AddRectFilled(
            ImVec2(pos.x - SCALE_BOX_SIZE, pos.y - SCALE_BOX_SIZE),
            ImVec2(pos.x + SCALE_BOX_SIZE, pos.y + SCALE_BOX_SIZE),
            color
        );
        draw_list->AddRect(
            ImVec2(pos.x - SCALE_BOX_SIZE, pos.y - SCALE_BOX_SIZE),
            ImVec2(pos.x + SCALE_BOX_SIZE, pos.y + SCALE_BOX_SIZE),
            IM_COL32(0, 0, 0, 150), 0.0f, 0, 1.0f
        );
    }
    
    // 4. Draw center sphere (for uniform scale / select)
    bool center_active = (m_active_op != GizmoOperation::None && m_active_axis == GizmoAxis::All);
    bool center_hovered = (m_hovered_axis == GizmoAxis::All);
    ImU32 center_color = (center_active || center_hovered) ? COLOR_CENTER_HOVER : COLOR_CENTER;
    draw_list->AddCircleFilled(ImVec2(center.x, center.y), CENTER_SIZE, center_color);
    draw_list->AddCircle(ImVec2(center.x, center.y), CENTER_SIZE, IM_COL32(0, 0, 0, 100), 12, 1.5f);
}

void GizmoPanel::HitTest(const vec2& mouse_pos, const vec2& center,
                          GizmoOperation& out_op, GizmoAxis& out_axis) {
    out_op = GizmoOperation::None;
    out_axis = GizmoAxis::None;
    
    // Helper: distance from point to line segment
    auto distToSegment = [](const vec2& p, const vec2& a, const vec2& b) -> float {
        vec2 ab(b.x - a.x, b.y - a.y);
        vec2 ap(p.x - a.x, p.y - a.y);
        float t = (ap.x * ab.x + ap.y * ab.y) / (ab.x * ab.x + ab.y * ab.y + 0.0001f);
        t = std::max(0.0f, std::min(1.0f, t));
        vec2 closest(a.x + t * ab.x, a.y + t * ab.y);
        vec2 diff(p.x - closest.x, p.y - closest.y);
        return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    };
    
    auto dist2D = [](const vec2& a, const vec2& b) -> float {
        float dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    };
    
    // Check center (uniform scale)
    if (dist2D(mouse_pos, center) < CENTER_SIZE + 4.0f) {
        out_op = GizmoOperation::Scale;
        out_axis = GizmoAxis::All;
        return;
    }
    
    // Check scale boxes first (they're on top)
    for (int i = 0; i < 3; i++) {
        vec2 box_pos = m_scale_box_pos[i];
        vec2 dir = m_axis_screen_dirs[i];
        box_pos.x += dir.x * 8.0f;
        box_pos.y += dir.y * 8.0f;
        
        if (std::abs(mouse_pos.x - box_pos.x) < SCALE_BOX_SIZE + 4.0f &&
            std::abs(mouse_pos.y - box_pos.y) < SCALE_BOX_SIZE + 4.0f) {
            out_op = GizmoOperation::Scale;
            out_axis = static_cast<GizmoAxis>(i + 1);
            return;
        }
    }
    
    // Check rotation ring
    float ring_dist = dist2D(mouse_pos, center);
    if (std::abs(ring_dist - ROTATION_RING_RADIUS) < HIT_TOLERANCE + 4.0f) {
        out_op = GizmoOperation::Rotate;
        // Determine which axis based on angle
        float angle = std::atan2(mouse_pos.y - center.y, mouse_pos.x - center.x);
        if (angle < 0) angle += 6.283f;
        int sector = (int)(angle / 2.094f) % 3;  // 3 sectors of 120 degrees
        out_axis = static_cast<GizmoAxis>(sector + 1);
        return;
    }
    
    // Check translation arrows
    for (int i = 0; i < 3; i++) {
        float dist = distToSegment(mouse_pos, center, m_axis_screen_ends[i]);
        if (dist < HIT_TOLERANCE) {
            out_op = GizmoOperation::Translate;
            out_axis = static_cast<GizmoAxis>(i + 1);
            return;
        }
    }
}

bool GizmoPanel::HandleInput(vec3& position, vec3& rotation, vec3& scale,
                              const vec2& center, const mat4& view, const mat4& proj) {
    ImGuiIO& io = ImGui::GetIO();
    vec2 mouse_pos(io.MousePos.x, io.MousePos.y);
    
    bool modified = false;
    m_drag_just_ended = false;
    
    // Update hover state when not dragging
    if (!m_is_dragging) {
        HitTest(mouse_pos, center, m_hovered_op, m_hovered_axis);
    }
    
    // Start drag - store original values for undo
    if (io.MouseClicked[0] && m_hovered_op != GizmoOperation::None) {
        m_active_op = m_hovered_op;
        m_active_axis = m_hovered_axis;
        m_is_dragging = true;
        m_last_mouse = mouse_pos;
        
        // Store original values for undo/redo
        m_drag_start_position = position;
        m_drag_start_rotation = rotation;
        m_drag_start_scale = scale;
        m_drag_made_changes = false;
        
        return false;
    }
    
    // Continue drag - apply incremental changes
    if (m_is_dragging && io.MouseDown[0]) {
        vec2 delta(mouse_pos.x - m_last_mouse.x, mouse_pos.y - m_last_mouse.y);
        m_last_mouse = mouse_pos;
        
        // Sensitivity
        float translate_speed = 0.02f;
        float rotate_speed = 0.5f;
        float scale_speed = 0.01f;
        
        int axis_idx = static_cast<int>(m_active_axis) - 1;  // 0=X, 1=Y, 2=Z
        
        switch (m_active_op) {
            case GizmoOperation::Translate: {
                if (axis_idx >= 0 && axis_idx < 3) {
                    vec2 axis_dir = m_axis_screen_dirs[axis_idx];
                    float proj_delta = delta.x * axis_dir.x + delta.y * axis_dir.y;
                    
                    // Apply to world position
                    switch (m_active_axis) {
                        case GizmoAxis::X: position.x += proj_delta * translate_speed; break;
                        case GizmoAxis::Y: position.y -= proj_delta * translate_speed; break;  // Y inverted
                        case GizmoAxis::Z: position.z += proj_delta * translate_speed; break;
                        default: break;
                    }
                    modified = true;
                    m_drag_made_changes = true;
                }
                break;
            }
            
            case GizmoOperation::Rotate: {
                // Use horizontal mouse movement for rotation
                float rot_delta = delta.x * rotate_speed;
                
                switch (m_active_axis) {
                    case GizmoAxis::X: rotation.x += rot_delta; break;
                    case GizmoAxis::Y: rotation.y += rot_delta; break;
                    case GizmoAxis::Z: rotation.z += rot_delta; break;
                    default: break;
                }
                modified = true;
                m_drag_made_changes = true;
                break;
            }
            
            case GizmoOperation::Scale: {
                float scale_delta = (delta.x - delta.y) * scale_speed;
                
                if (m_active_axis == GizmoAxis::All) {
                    // Uniform scale
                    scale.x = std::max(0.01f, scale.x + scale_delta);
                    scale.y = std::max(0.01f, scale.y + scale_delta);
                    scale.z = std::max(0.01f, scale.z + scale_delta);
                } else if (axis_idx >= 0 && axis_idx < 3) {
                    switch (m_active_axis) {
                        case GizmoAxis::X: scale.x = std::max(0.01f, scale.x + scale_delta); break;
                        case GizmoAxis::Y: scale.y = std::max(0.01f, scale.y + scale_delta); break;
                        case GizmoAxis::Z: scale.z = std::max(0.01f, scale.z + scale_delta); break;
                        default: break;
                    }
                }
                modified = true;
                m_drag_made_changes = true;
                break;
            }
            
            default:
                break;
        }
    }
    
    // End drag - signal that we should create an undo command
    if (io.MouseReleased[0] && m_is_dragging) {
        m_drag_just_ended = m_drag_made_changes;  // Only signal if changes were made
        m_is_dragging = false;
        m_active_op = GizmoOperation::None;
        m_active_axis = GizmoAxis::None;
    }
    
    return modified;
}

} // namespace action
