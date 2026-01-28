#pragma once

#include "shader_graph.h"
#include "core/types.h"

namespace action {

class Renderer;

/*
 * ShaderGraphEditor - Visual node-based material editor panel
 * 
 * Features:
 * - Node creation/deletion
 * - Link connections via drag
 * - Pan/zoom canvas
 * - Node selection (single/multi)
 * - Grid background
 * - Minimap (optional)
 */
class ShaderGraphEditor {
public:
    ShaderGraphEditor() = default;
    ~ShaderGraphEditor() = default;
    
    void Initialize();
    void Draw(Renderer& renderer);
    
    // Graph access
    ShaderGraph& GetGraph() { return m_graph; }
    const ShaderGraph& GetGraph() const { return m_graph; }
    
    // New/Load/Save
    void NewGraph();
    bool LoadGraph(const std::string& path);
    bool SaveGraph(const std::string& path);
    
    bool visible = true;
    
private:
    void DrawToolbar();
    void DrawCanvas();
    void DrawNodes();
    void DrawNode(ShaderNode& node);
    void DrawLinks();
    void DrawLink(const ShaderLink& link);
    void DrawContextMenu();
    void DrawNodeContextMenu();
    void DrawPinTooltip(const ShaderPin& pin);
    void DrawMinimap();
    void DrawCodePreview();
    
    // Interaction
    void HandleCanvasInput();
    void HandleNodeDragging();
    void HandleLinkDragging();
    void HandleSelection();
    
    // Coordinate conversion
    vec2 ScreenToCanvas(const vec2& screen_pos) const;
    vec2 CanvasToScreen(const vec2& canvas_pos) const;
    
    // Pin positions
    vec2 GetPinPosition(const ShaderNode& node, const ShaderPin& pin) const;
    
    // Hit testing
    ShaderNode* HitTestNode(const vec2& canvas_pos);
    ShaderPin* HitTestPin(const vec2& canvas_pos, ShaderNode** out_node = nullptr);
    
    ShaderGraph m_graph;
    
    // Canvas state
    vec2 m_canvas_offset{0, 0};  // Pan offset
    float m_canvas_zoom = 1.0f;
    vec2 m_canvas_screen_pos{0, 0};
    vec2 m_canvas_size{800, 600};
    
    // Interaction state
    bool m_dragging_node = false;
    bool m_dragging_canvas = false;
    bool m_dragging_link = false;
    bool m_box_selecting = false;
    
    vec2 m_drag_start{0, 0};
    vec2 m_selection_start{0, 0};
    
    // Link creation
    u32 m_link_start_pin = 0;
    vec2 m_link_end_pos{0, 0};
    bool m_link_from_input = false;
    
    // Context menu
    bool m_show_context_menu = false;
    bool m_show_node_context_menu = false;
    vec2 m_context_menu_pos{0, 0};
    u32 m_context_node_id = 0;
    
    // UI state
    bool m_show_minimap = true;
    bool m_show_grid = true;
    bool m_show_code_preview = false;
    bool m_snap_to_grid = false;
    float m_grid_size = 20.0f;
    
    // Hovered
    u32 m_hovered_node_id = 0;
    u32 m_hovered_pin_id = 0;
};

} // namespace action
