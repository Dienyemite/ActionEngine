#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace action {

/*
 * Shader Graph - Visual Material/Shader Editor
 * 
 * Node-based editor for creating materials visually, similar to:
 * - Unreal Engine Material Editor
 * - Blender Shader Nodes
 * - Unity Shader Graph
 */

// Data types for shader graph pins
enum class ShaderPinType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Texture2D,
    Sampler,
    Matrix4
};

// Pin direction
enum class ShaderPinDirection {
    Input,
    Output
};

// Forward declarations
struct ShaderNode;
struct ShaderLink;
class ShaderGraph;

/*
 * ShaderPin - Input or output connection point on a node
 */
struct ShaderPin {
    u32 id = 0;
    std::string name;
    ShaderPinType type = ShaderPinType::Float;
    ShaderPinDirection direction = ShaderPinDirection::Input;
    u32 node_id = 0;  // Parent node
    
    // Default value (for inputs without connections)
    union {
        float value_float;
        float value_vec2[2];
        float value_vec3[3];
        float value_vec4[4];
    };
    
    // Position for rendering (relative to node)
    vec2 position{0, 0};
    
    // Connection state
    bool connected = false;
    
    ShaderPin() : value_float(0.0f) {
        value_vec4[0] = value_vec4[1] = value_vec4[2] = value_vec4[3] = 0.0f;
    }
    
    // Get color based on type
    u32 GetTypeColor() const;
    
    // Get type name
    const char* GetTypeName() const;
};

/*
 * ShaderNode - A node in the shader graph
 */
enum class ShaderNodeType {
    // Output
    MaterialOutput,
    
    // Constants/Inputs
    ConstantFloat,
    ConstantVec2,
    ConstantVec3,
    ConstantVec4,
    ConstantColor,
    TextureSample,
    Time,
    VertexPosition,
    VertexNormal,
    VertexUV,
    VertexColor,
    
    // Math Operations
    Add,
    Subtract,
    Multiply,
    Divide,
    Power,
    Sqrt,
    Abs,
    Negate,
    
    // Vector Operations
    Dot,
    Cross,
    Normalize,
    Length,
    Distance,
    Reflect,
    Lerp,
    Clamp,
    Saturate,
    
    // Trigonometry
    Sin,
    Cos,
    Tan,
    
    // Color Operations
    RGB_to_HSV,
    HSV_to_RGB,
    
    // Utility
    Split,      // Vec4 -> separate components
    Combine,    // Separate components -> Vec4
    Fresnel,
    
    // Comment (non-functional, just for organization)
    Comment
};

struct ShaderNode {
    u32 id = 0;
    std::string title;
    ShaderNodeType type = ShaderNodeType::ConstantFloat;
    
    std::vector<ShaderPin> inputs;
    std::vector<ShaderPin> outputs;
    
    // Position in graph
    vec2 position{0, 0};
    vec2 size{150, 100};
    
    // UI state
    bool selected = false;
    bool collapsed = false;
    
    // For comment nodes
    std::string comment_text;
    vec3 comment_color{0.3f, 0.3f, 0.3f};
    
    // Get node display color
    u32 GetHeaderColor() const;
    
    // Create standard inputs/outputs based on type
    void SetupPinsForType();
};

/*
 * ShaderLink - Connection between two pins
 */
struct ShaderLink {
    u32 id = 0;
    u32 from_node = 0;
    u32 from_pin = 0;
    u32 to_node = 0;
    u32 to_pin = 0;
    
    // Cached for rendering
    vec2 from_pos{0, 0};
    vec2 to_pos{0, 0};
};

/*
 * ShaderGraph - Complete material graph
 */
class ShaderGraph {
public:
    ShaderGraph() = default;
    ~ShaderGraph() = default;
    
    void Initialize();
    void Clear();
    
    // Node management
    ShaderNode* AddNode(ShaderNodeType type, const vec2& position);
    void DeleteNode(u32 node_id);
    ShaderNode* GetNode(u32 node_id);
    const ShaderNode* GetNode(u32 node_id) const;
    
    // Link management
    bool CanConnect(u32 from_pin_id, u32 to_pin_id) const;
    ShaderLink* AddLink(u32 from_pin_id, u32 to_pin_id);
    void DeleteLink(u32 link_id);
    void DeleteLinksToPin(u32 pin_id);
    
    // Find pin/node by ID
    ShaderPin* FindPin(u32 pin_id);
    const ShaderPin* FindPin(u32 pin_id) const;
    ShaderNode* FindPinOwner(u32 pin_id);
    
    // Selection
    void ClearSelection();
    void SelectNode(u32 node_id, bool add_to_selection = false);
    void SelectNodesInRect(const vec2& min, const vec2& max);
    std::vector<u32> GetSelectedNodeIds() const;
    
    // Code generation
    std::string GenerateGLSL() const;
    std::string GenerateSPIRVInfo() const;
    
    // Serialization
    std::string ToJson() const;
    bool FromJson(const std::string& json);
    
    // Accessors
    std::vector<ShaderNode>& GetNodes() { return m_nodes; }
    const std::vector<ShaderNode>& GetNodes() const { return m_nodes; }
    std::vector<ShaderLink>& GetLinks() { return m_links; }
    const std::vector<ShaderLink>& GetLinks() const { return m_links; }
    
    std::string name = "Untitled Material";
    
private:
    std::vector<ShaderNode> m_nodes;
    std::vector<ShaderLink> m_links;
    u32 m_next_node_id = 1;
    u32 m_next_pin_id = 1;
    u32 m_next_link_id = 1;
};

} // namespace action
