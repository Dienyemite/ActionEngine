#include "shader_graph.h"
#include "core/logging.h"
#include <sstream>
#include <algorithm>

namespace action {

// Pin type colors (ImGui packed color format)
u32 ShaderPin::GetTypeColor() const {
    switch (type) {
        case ShaderPinType::Float:    return 0xFF00FF00;  // Green
        case ShaderPinType::Vec2:     return 0xFF00FFFF;  // Cyan
        case ShaderPinType::Vec3:     return 0xFFFFFF00;  // Yellow
        case ShaderPinType::Vec4:     return 0xFFFF00FF;  // Magenta
        case ShaderPinType::Color:    return 0xFFFFFFFF;  // White
        case ShaderPinType::Texture2D: return 0xFF0080FF; // Orange
        case ShaderPinType::Sampler:  return 0xFF8080FF;  // Pink
        case ShaderPinType::Matrix4:  return 0xFF808080;  // Gray
        default:                      return 0xFFFFFFFF;
    }
}

const char* ShaderPin::GetTypeName() const {
    switch (type) {
        case ShaderPinType::Float:     return "Float";
        case ShaderPinType::Vec2:      return "Vec2";
        case ShaderPinType::Vec3:      return "Vec3";
        case ShaderPinType::Vec4:      return "Vec4";
        case ShaderPinType::Color:     return "Color";
        case ShaderPinType::Texture2D: return "Texture2D";
        case ShaderPinType::Sampler:   return "Sampler";
        case ShaderPinType::Matrix4:   return "Matrix4";
        default:                       return "Unknown";
    }
}

u32 ShaderNode::GetHeaderColor() const {
    // Color based on node category
    switch (type) {
        case ShaderNodeType::MaterialOutput:
            return 0xFF404080;  // Purple for output
            
        case ShaderNodeType::ConstantFloat:
        case ShaderNodeType::ConstantVec2:
        case ShaderNodeType::ConstantVec3:
        case ShaderNodeType::ConstantVec4:
        case ShaderNodeType::ConstantColor:
            return 0xFF408040;  // Green for constants
            
        case ShaderNodeType::TextureSample:
            return 0xFF804040;  // Red for textures
            
        case ShaderNodeType::Time:
        case ShaderNodeType::VertexPosition:
        case ShaderNodeType::VertexNormal:
        case ShaderNodeType::VertexUV:
        case ShaderNodeType::VertexColor:
            return 0xFF604080;  // Purple for vertex attributes
            
        case ShaderNodeType::Add:
        case ShaderNodeType::Subtract:
        case ShaderNodeType::Multiply:
        case ShaderNodeType::Divide:
        case ShaderNodeType::Power:
        case ShaderNodeType::Sqrt:
        case ShaderNodeType::Abs:
        case ShaderNodeType::Negate:
            return 0xFF606060;  // Gray for math
            
        case ShaderNodeType::Dot:
        case ShaderNodeType::Cross:
        case ShaderNodeType::Normalize:
        case ShaderNodeType::Length:
        case ShaderNodeType::Distance:
        case ShaderNodeType::Reflect:
        case ShaderNodeType::Lerp:
        case ShaderNodeType::Clamp:
        case ShaderNodeType::Saturate:
            return 0xFF606080;  // Blue-gray for vector ops
            
        case ShaderNodeType::Sin:
        case ShaderNodeType::Cos:
        case ShaderNodeType::Tan:
            return 0xFF408080;  // Teal for trig
            
        case ShaderNodeType::RGB_to_HSV:
        case ShaderNodeType::HSV_to_RGB:
            return 0xFF804080;  // Magenta for color
            
        case ShaderNodeType::Split:
        case ShaderNodeType::Combine:
        case ShaderNodeType::Fresnel:
            return 0xFF606060;  // Gray for utility
            
        case ShaderNodeType::Comment:
            return 0xFF404040;  // Dark gray for comments
            
        default:
            return 0xFF505050;
    }
}

void ShaderNode::SetupPinsForType() {
    inputs.clear();
    outputs.clear();
    
    auto addInput = [this](const std::string& name, ShaderPinType type) {
        ShaderPin pin;
        pin.name = name;
        pin.type = type;
        pin.direction = ShaderPinDirection::Input;
        pin.node_id = id;
        inputs.push_back(pin);
    };
    
    auto addOutput = [this](const std::string& name, ShaderPinType type) {
        ShaderPin pin;
        pin.name = name;
        pin.type = type;
        pin.direction = ShaderPinDirection::Output;
        pin.node_id = id;
        outputs.push_back(pin);
    };
    
    switch (type) {
        case ShaderNodeType::MaterialOutput:
            title = "Material Output";
            addInput("Base Color", ShaderPinType::Color);
            addInput("Metallic", ShaderPinType::Float);
            addInput("Roughness", ShaderPinType::Float);
            addInput("Normal", ShaderPinType::Vec3);
            addInput("Emissive", ShaderPinType::Color);
            addInput("Opacity", ShaderPinType::Float);
            size = {180, 160};
            break;
            
        case ShaderNodeType::ConstantFloat:
            title = "Float";
            addOutput("Value", ShaderPinType::Float);
            size = {120, 60};
            break;
            
        case ShaderNodeType::ConstantVec2:
            title = "Vector2";
            addOutput("Value", ShaderPinType::Vec2);
            size = {120, 80};
            break;
            
        case ShaderNodeType::ConstantVec3:
            title = "Vector3";
            addOutput("Value", ShaderPinType::Vec3);
            size = {120, 100};
            break;
            
        case ShaderNodeType::ConstantVec4:
            title = "Vector4";
            addOutput("Value", ShaderPinType::Vec4);
            size = {120, 120};
            break;
            
        case ShaderNodeType::ConstantColor:
            title = "Color";
            addOutput("Color", ShaderPinType::Color);
            addOutput("R", ShaderPinType::Float);
            addOutput("G", ShaderPinType::Float);
            addOutput("B", ShaderPinType::Float);
            addOutput("A", ShaderPinType::Float);
            size = {140, 140};
            break;
            
        case ShaderNodeType::TextureSample:
            title = "Texture Sample";
            addInput("Texture", ShaderPinType::Texture2D);
            addInput("UV", ShaderPinType::Vec2);
            addOutput("Color", ShaderPinType::Color);
            addOutput("R", ShaderPinType::Float);
            addOutput("G", ShaderPinType::Float);
            addOutput("B", ShaderPinType::Float);
            addOutput("A", ShaderPinType::Float);
            size = {160, 160};
            break;
            
        case ShaderNodeType::Time:
            title = "Time";
            addOutput("Time", ShaderPinType::Float);
            addOutput("Sin Time", ShaderPinType::Float);
            addOutput("Cos Time", ShaderPinType::Float);
            size = {140, 100};
            break;
            
        case ShaderNodeType::VertexPosition:
            title = "Vertex Position";
            addOutput("World", ShaderPinType::Vec3);
            addOutput("Local", ShaderPinType::Vec3);
            size = {140, 80};
            break;
            
        case ShaderNodeType::VertexNormal:
            title = "Vertex Normal";
            addOutput("World", ShaderPinType::Vec3);
            addOutput("Local", ShaderPinType::Vec3);
            size = {140, 80};
            break;
            
        case ShaderNodeType::VertexUV:
            title = "UV";
            addOutput("UV0", ShaderPinType::Vec2);
            addOutput("UV1", ShaderPinType::Vec2);
            size = {100, 80};
            break;
            
        case ShaderNodeType::VertexColor:
            title = "Vertex Color";
            addOutput("Color", ShaderPinType::Color);
            size = {140, 60};
            break;
            
        case ShaderNodeType::Add:
            title = "Add";
            addInput("A", ShaderPinType::Vec4);
            addInput("B", ShaderPinType::Vec4);
            addOutput("Result", ShaderPinType::Vec4);
            size = {100, 80};
            break;
            
        case ShaderNodeType::Subtract:
            title = "Subtract";
            addInput("A", ShaderPinType::Vec4);
            addInput("B", ShaderPinType::Vec4);
            addOutput("Result", ShaderPinType::Vec4);
            size = {100, 80};
            break;
            
        case ShaderNodeType::Multiply:
            title = "Multiply";
            addInput("A", ShaderPinType::Vec4);
            addInput("B", ShaderPinType::Vec4);
            addOutput("Result", ShaderPinType::Vec4);
            size = {100, 80};
            break;
            
        case ShaderNodeType::Divide:
            title = "Divide";
            addInput("A", ShaderPinType::Vec4);
            addInput("B", ShaderPinType::Vec4);
            addOutput("Result", ShaderPinType::Vec4);
            size = {100, 80};
            break;
            
        case ShaderNodeType::Lerp:
            title = "Lerp";
            addInput("A", ShaderPinType::Vec4);
            addInput("B", ShaderPinType::Vec4);
            addInput("T", ShaderPinType::Float);
            addOutput("Result", ShaderPinType::Vec4);
            size = {100, 100};
            break;
            
        case ShaderNodeType::Clamp:
            title = "Clamp";
            addInput("Value", ShaderPinType::Float);
            addInput("Min", ShaderPinType::Float);
            addInput("Max", ShaderPinType::Float);
            addOutput("Result", ShaderPinType::Float);
            size = {100, 100};
            break;
            
        case ShaderNodeType::Saturate:
            title = "Saturate";
            addInput("Value", ShaderPinType::Vec4);
            addOutput("Result", ShaderPinType::Vec4);
            size = {100, 60};
            break;
            
        case ShaderNodeType::Dot:
            title = "Dot Product";
            addInput("A", ShaderPinType::Vec3);
            addInput("B", ShaderPinType::Vec3);
            addOutput("Result", ShaderPinType::Float);
            size = {120, 80};
            break;
            
        case ShaderNodeType::Cross:
            title = "Cross Product";
            addInput("A", ShaderPinType::Vec3);
            addInput("B", ShaderPinType::Vec3);
            addOutput("Result", ShaderPinType::Vec3);
            size = {120, 80};
            break;
            
        case ShaderNodeType::Normalize:
            title = "Normalize";
            addInput("Value", ShaderPinType::Vec3);
            addOutput("Result", ShaderPinType::Vec3);
            size = {100, 60};
            break;
            
        case ShaderNodeType::Length:
            title = "Length";
            addInput("Value", ShaderPinType::Vec3);
            addOutput("Result", ShaderPinType::Float);
            size = {100, 60};
            break;
            
        case ShaderNodeType::Sin:
            title = "Sin";
            addInput("Value", ShaderPinType::Float);
            addOutput("Result", ShaderPinType::Float);
            size = {80, 60};
            break;
            
        case ShaderNodeType::Cos:
            title = "Cos";
            addInput("Value", ShaderPinType::Float);
            addOutput("Result", ShaderPinType::Float);
            size = {80, 60};
            break;
            
        case ShaderNodeType::Split:
            title = "Split";
            addInput("Vector", ShaderPinType::Vec4);
            addOutput("X", ShaderPinType::Float);
            addOutput("Y", ShaderPinType::Float);
            addOutput("Z", ShaderPinType::Float);
            addOutput("W", ShaderPinType::Float);
            size = {100, 120};
            break;
            
        case ShaderNodeType::Combine:
            title = "Combine";
            addInput("X", ShaderPinType::Float);
            addInput("Y", ShaderPinType::Float);
            addInput("Z", ShaderPinType::Float);
            addInput("W", ShaderPinType::Float);
            addOutput("Vector", ShaderPinType::Vec4);
            size = {100, 120};
            break;
            
        case ShaderNodeType::Fresnel:
            title = "Fresnel";
            addInput("Normal", ShaderPinType::Vec3);
            addInput("Power", ShaderPinType::Float);
            addOutput("Result", ShaderPinType::Float);
            size = {100, 80};
            break;
            
        case ShaderNodeType::Comment:
            title = "Comment";
            size = {200, 100};
            break;
            
        default:
            title = "Unknown";
            size = {100, 60};
            break;
    }
}

void ShaderGraph::Initialize() {
    Clear();
    
    // Add default material output node
    AddNode(ShaderNodeType::MaterialOutput, {400, 200});
    
    LOG_INFO("ShaderGraph initialized");
}

void ShaderGraph::Clear() {
    m_nodes.clear();
    m_links.clear();
    m_next_node_id = 1;
    m_next_pin_id = 1;
    m_next_link_id = 1;
}

ShaderNode* ShaderGraph::AddNode(ShaderNodeType type, const vec2& position) {
    ShaderNode node;
    node.id = m_next_node_id++;
    node.type = type;
    node.position = position;
    node.SetupPinsForType();
    
    // Assign pin IDs
    for (auto& pin : node.inputs) {
        pin.id = m_next_pin_id++;
    }
    for (auto& pin : node.outputs) {
        pin.id = m_next_pin_id++;
    }
    
    m_nodes.push_back(node);
    
    LOG_INFO("Added shader node: {} (ID: {})", node.title, node.id);
    return &m_nodes.back();
}

void ShaderGraph::DeleteNode(u32 node_id) {
    // Don't allow deleting the material output
    if (auto* node = GetNode(node_id)) {
        if (node->type == ShaderNodeType::MaterialOutput) {
            LOG_WARN("Cannot delete Material Output node");
            return;
        }
    }
    
    // Delete all links connected to this node
    for (auto& node : m_nodes) {
        if (node.id == node_id) {
            for (auto& pin : node.inputs) {
                DeleteLinksToPin(pin.id);
            }
            for (auto& pin : node.outputs) {
                DeleteLinksToPin(pin.id);
            }
            break;
        }
    }
    
    // Remove the node
    m_nodes.erase(
        std::remove_if(m_nodes.begin(), m_nodes.end(),
                       [node_id](const ShaderNode& n) { return n.id == node_id; }),
        m_nodes.end()
    );
    
    LOG_INFO("Deleted shader node: {}", node_id);
}

ShaderNode* ShaderGraph::GetNode(u32 node_id) {
    for (auto& node : m_nodes) {
        if (node.id == node_id) return &node;
    }
    return nullptr;
}

const ShaderNode* ShaderGraph::GetNode(u32 node_id) const {
    for (const auto& node : m_nodes) {
        if (node.id == node_id) return &node;
    }
    return nullptr;
}

bool ShaderGraph::CanConnect(u32 from_pin_id, u32 to_pin_id) const {
    const ShaderPin* from = FindPin(from_pin_id);
    const ShaderPin* to = FindPin(to_pin_id);
    
    if (!from || !to) return false;
    
    // Must connect output to input
    if (from->direction != ShaderPinDirection::Output) return false;
    if (to->direction != ShaderPinDirection::Input) return false;
    
    // Can't connect to same node
    if (from->node_id == to->node_id) return false;
    
    // For now, allow any type connection (implicit conversion)
    // A more advanced system would check type compatibility
    
    return true;
}

ShaderLink* ShaderGraph::AddLink(u32 from_pin_id, u32 to_pin_id) {
    if (!CanConnect(from_pin_id, to_pin_id)) {
        LOG_WARN("Cannot connect pins {} -> {}", from_pin_id, to_pin_id);
        return nullptr;
    }
    
    // Remove any existing link to the input pin (inputs can only have one connection)
    DeleteLinksToPin(to_pin_id);
    
    ShaderPin* from = FindPin(from_pin_id);
    ShaderPin* to = FindPin(to_pin_id);
    
    ShaderLink link;
    link.id = m_next_link_id++;
    link.from_node = from->node_id;
    link.from_pin = from_pin_id;
    link.to_node = to->node_id;
    link.to_pin = to_pin_id;
    
    // Mark pins as connected
    from->connected = true;
    to->connected = true;
    
    m_links.push_back(link);
    
    LOG_INFO("Added shader link: {} -> {}", from_pin_id, to_pin_id);
    return &m_links.back();
}

void ShaderGraph::DeleteLink(u32 link_id) {
    for (auto it = m_links.begin(); it != m_links.end(); ++it) {
        if (it->id == link_id) {
            // Update connection status
            if (ShaderPin* from = FindPin(it->from_pin)) {
                // Check if output has any other connections
                bool has_other = false;
                for (const auto& l : m_links) {
                    if (l.id != link_id && l.from_pin == it->from_pin) {
                        has_other = true;
                        break;
                    }
                }
                if (!has_other) from->connected = false;
            }
            if (ShaderPin* to = FindPin(it->to_pin)) {
                to->connected = false;
            }
            
            m_links.erase(it);
            LOG_INFO("Deleted shader link: {}", link_id);
            return;
        }
    }
}

void ShaderGraph::DeleteLinksToPin(u32 pin_id) {
    std::vector<u32> to_delete;
    for (const auto& link : m_links) {
        if (link.from_pin == pin_id || link.to_pin == pin_id) {
            to_delete.push_back(link.id);
        }
    }
    for (u32 id : to_delete) {
        DeleteLink(id);
    }
}

ShaderPin* ShaderGraph::FindPin(u32 pin_id) {
    for (auto& node : m_nodes) {
        for (auto& pin : node.inputs) {
            if (pin.id == pin_id) return &pin;
        }
        for (auto& pin : node.outputs) {
            if (pin.id == pin_id) return &pin;
        }
    }
    return nullptr;
}

const ShaderPin* ShaderGraph::FindPin(u32 pin_id) const {
    for (const auto& node : m_nodes) {
        for (const auto& pin : node.inputs) {
            if (pin.id == pin_id) return &pin;
        }
        for (const auto& pin : node.outputs) {
            if (pin.id == pin_id) return &pin;
        }
    }
    return nullptr;
}

ShaderNode* ShaderGraph::FindPinOwner(u32 pin_id) {
    for (auto& node : m_nodes) {
        for (const auto& pin : node.inputs) {
            if (pin.id == pin_id) return &node;
        }
        for (const auto& pin : node.outputs) {
            if (pin.id == pin_id) return &node;
        }
    }
    return nullptr;
}

void ShaderGraph::ClearSelection() {
    for (auto& node : m_nodes) {
        node.selected = false;
    }
}

void ShaderGraph::SelectNode(u32 node_id, bool add_to_selection) {
    if (!add_to_selection) {
        ClearSelection();
    }
    if (ShaderNode* node = GetNode(node_id)) {
        node->selected = true;
    }
}

void ShaderGraph::SelectNodesInRect(const vec2& min, const vec2& max) {
    for (auto& node : m_nodes) {
        // Check if node center is in rect
        vec2 center = node.position + node.size * 0.5f;
        if (center.x >= min.x && center.x <= max.x &&
            center.y >= min.y && center.y <= max.y) {
            node.selected = true;
        }
    }
}

std::vector<u32> ShaderGraph::GetSelectedNodeIds() const {
    std::vector<u32> selected;
    for (const auto& node : m_nodes) {
        if (node.selected) {
            selected.push_back(node.id);
        }
    }
    return selected;
}

std::string ShaderGraph::GenerateGLSL() const {
    std::stringstream ss;
    
    ss << "// Generated by ActionEngine Shader Graph\n";
    ss << "// Material: " << name << "\n\n";
    
    ss << "#version 450\n\n";
    
    ss << "// Inputs\n";
    ss << "layout(location = 0) in vec3 fragPosition;\n";
    ss << "layout(location = 1) in vec3 fragNormal;\n";
    ss << "layout(location = 2) in vec2 fragUV;\n";
    ss << "layout(location = 3) in vec3 fragColor;\n\n";
    
    ss << "// Outputs\n";
    ss << "layout(location = 0) out vec4 outColor;\n\n";
    
    ss << "// Uniforms\n";
    ss << "layout(binding = 0) uniform MaterialData {\n";
    ss << "    float time;\n";
    ss << "    // Add more uniforms as needed\n";
    ss << "} material;\n\n";
    
    ss << "void main() {\n";
    ss << "    // TODO: Generate actual shader code from graph\n";
    ss << "    vec3 baseColor = vec3(0.8, 0.8, 0.8);\n";
    ss << "    float metallic = 0.0;\n";
    ss << "    float roughness = 0.5;\n";
    ss << "    vec3 normal = normalize(fragNormal);\n";
    ss << "    vec3 emissive = vec3(0.0);\n";
    ss << "    float opacity = 1.0;\n\n";
    ss << "    // Simple output\n";
    ss << "    outColor = vec4(baseColor, opacity);\n";
    ss << "}\n";
    
    return ss.str();
}

std::string ShaderGraph::GenerateSPIRVInfo() const {
    std::stringstream ss;
    ss << "Shader Graph: " << name << "\n";
    ss << "Nodes: " << m_nodes.size() << "\n";
    ss << "Links: " << m_links.size() << "\n";
    ss << "\nSPIR-V generation would require glslang or shaderc integration.\n";
    return ss.str();
}

std::string ShaderGraph::ToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << name << "\",\n";
    ss << "  \"nodes\": [\n";
    
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const auto& node = m_nodes[i];
        ss << "    {\n";
        ss << "      \"id\": " << node.id << ",\n";
        ss << "      \"type\": " << static_cast<int>(node.type) << ",\n";
        ss << "      \"title\": \"" << node.title << "\",\n";
        ss << "      \"position\": [" << node.position.x << ", " << node.position.y << "]\n";
        ss << "    }" << (i < m_nodes.size() - 1 ? "," : "") << "\n";
    }
    
    ss << "  ],\n";
    ss << "  \"links\": [\n";
    
    for (size_t i = 0; i < m_links.size(); ++i) {
        const auto& link = m_links[i];
        ss << "    {\n";
        ss << "      \"id\": " << link.id << ",\n";
        ss << "      \"from_pin\": " << link.from_pin << ",\n";
        ss << "      \"to_pin\": " << link.to_pin << "\n";
        ss << "    }" << (i < m_links.size() - 1 ? "," : "") << "\n";
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

bool ShaderGraph::FromJson(const std::string& json) {
    // TODO: Implement JSON parsing
    LOG_WARN("ShaderGraph::FromJson not yet implemented");
    return false;
}

} // namespace action
