#include "editor_commands.h"
#include "editor/editor.h"

namespace action {

// ============================================================================
// TransformCommand
// ============================================================================

TransformCommand::TransformCommand(Editor* editor, u64 node_id, Type type,
                                   const vec3& old_value, const vec3& new_value)
    : m_editor(editor)
    , m_node_id(node_id)
    , m_type(type)
    , m_old_value(old_value)
    , m_new_value(new_value)
{
}

void TransformCommand::Execute()
{
    switch (m_type) {
        case Type::Position:
            m_editor->SetNodePosition(m_node_id, m_new_value);
            break;
        case Type::Rotation:
            m_editor->SetNodeRotation(m_node_id, m_new_value);
            break;
        case Type::Scale:
            m_editor->SetNodeScale(m_node_id, m_new_value);
            break;
    }
}

void TransformCommand::Undo()
{
    switch (m_type) {
        case Type::Position:
            m_editor->SetNodePosition(m_node_id, m_old_value);
            break;
        case Type::Rotation:
            m_editor->SetNodeRotation(m_node_id, m_old_value);
            break;
        case Type::Scale:
            m_editor->SetNodeScale(m_node_id, m_old_value);
            break;
    }
}

std::string TransformCommand::GetDescription() const
{
    switch (m_type) {
        case Type::Position: return "Move Object";
        case Type::Rotation: return "Rotate Object";
        case Type::Scale: return "Scale Object";
    }
    return "Transform";
}

bool TransformCommand::CanMergeWith(const Command* other) const
{
    if (other->GetTypeId() != GetTypeId()) return false;
    auto* t = static_cast<const TransformCommand*>(other);
    return t->m_node_id == m_node_id && t->m_type == m_type;
}

void TransformCommand::MergeWith(const Command* other)
{
    auto* t = static_cast<const TransformCommand*>(other);
    m_new_value = t->m_new_value;
}

// ============================================================================
// AddNodeCommand
// ============================================================================

AddNodeCommand::AddNodeCommand(Editor* editor, const std::string& name, u64 parent_id)
    : m_editor(editor)
    , m_name(name)
    , m_parent_id(parent_id)
    , m_position(0, 0, 0)
    , m_rotation(0, 0, 0)
    , m_scale(1, 1, 1)
    , m_color(0.8f, 0.8f, 0.8f)
{
}

void AddNodeCommand::Execute()
{
    EditorNode* parent = nullptr;
    if (m_parent_id != 0) {
        parent = m_editor->FindNode(m_parent_id);
    }
    
    EditorNode* node = m_editor->AddNode(m_name, parent);
    if (node) {
        m_created_id = node->id;
        // Restore transform if redoing
        node->position = m_position;
        node->rotation = m_rotation;
        node->scale = m_scale;
        node->color = m_color;
    }
}

void AddNodeCommand::Undo()
{
    if (m_created_id != 0) {
        // Store current state before deletion
        EditorNode* node = m_editor->FindNode(m_created_id);
        if (node) {
            m_position = node->position;
            m_rotation = node->rotation;
            m_scale = node->scale;
            m_color = node->color;
        }
        m_editor->DeleteNode(static_cast<u32>(m_created_id));
    }
}

std::string AddNodeCommand::GetDescription() const
{
    return "Add " + m_name;
}

// ============================================================================
// DeleteNodeCommand
// ============================================================================

DeleteNodeCommand::DeleteNodeCommand(Editor* editor, u64 node_id)
    : m_editor(editor)
    , m_node_id(node_id)
    , m_parent_id(0)
    , m_position(0, 0, 0)
    , m_rotation(0, 0, 0)
    , m_scale(1, 1, 1)
    , m_color(0.8f, 0.8f, 0.8f)
{
    // Store node data immediately
    EditorNode* node = m_editor->FindNode(m_node_id);
    if (node) {
        m_name = node->type;
        m_position = node->position;
        m_rotation = node->rotation;
        m_scale = node->scale;
        m_color = node->color;
        // TODO: Store parent ID properly
    }
}

void DeleteNodeCommand::Execute()
{
    // Store node data before deletion for undo
    EditorNode* node = m_editor->FindNode(m_node_id);
    if (node) {
        m_name = node->type;
        m_position = node->position;
        m_rotation = node->rotation;
        m_scale = node->scale;
        m_color = node->color;
    }
    m_editor->DeleteNode(static_cast<u32>(m_node_id));
}

void DeleteNodeCommand::Undo()
{
    // Recreate the node
    EditorNode* parent = nullptr;
    if (m_parent_id != 0) {
        parent = m_editor->FindNode(m_parent_id);
    }
    
    EditorNode* node = m_editor->AddNode(m_name, parent);
    if (node) {
        // Override the ID to restore original
        // Note: This may cause issues if IDs are not managed carefully
        node->position = m_position;
        node->rotation = m_rotation;
        node->scale = m_scale;
        node->color = m_color;
    }
}

std::string DeleteNodeCommand::GetDescription() const
{
    return "Delete " + m_name;
}

// ============================================================================
// RenameNodeCommand
// ============================================================================

RenameNodeCommand::RenameNodeCommand(Editor* editor, u64 node_id,
                                     const std::string& old_name, const std::string& new_name)
    : m_editor(editor)
    , m_node_id(node_id)
    , m_old_name(old_name)
    , m_new_name(new_name)
{
}

void RenameNodeCommand::Execute()
{
    m_editor->SetNodeName(m_node_id, m_new_name);
}

void RenameNodeCommand::Undo()
{
    m_editor->SetNodeName(m_node_id, m_old_name);
}

std::string RenameNodeCommand::GetDescription() const
{
    return "Rename to " + m_new_name;
}

// ============================================================================
// ColorChangeCommand
// ============================================================================

ColorChangeCommand::ColorChangeCommand(Editor* editor, u64 node_id,
                                       const vec3& old_color, const vec3& new_color)
    : m_editor(editor)
    , m_node_id(node_id)
    , m_old_color(old_color)
    , m_new_color(new_color)
{
}

void ColorChangeCommand::Execute()
{
    m_editor->SetNodeColor(m_node_id, m_new_color);
}

void ColorChangeCommand::Undo()
{
    m_editor->SetNodeColor(m_node_id, m_old_color);
}

std::string ColorChangeCommand::GetDescription() const
{
    return "Change Color";
}

bool ColorChangeCommand::CanMergeWith(const Command* other) const
{
    if (other->GetTypeId() != GetTypeId()) return false;
    auto* c = static_cast<const ColorChangeCommand*>(other);
    return c->m_node_id == m_node_id;
}

void ColorChangeCommand::MergeWith(const Command* other)
{
    auto* c = static_cast<const ColorChangeCommand*>(other);
    m_new_color = c->m_new_color;
}

// ============================================================================
// ReparentNodeCommand
// ============================================================================

ReparentNodeCommand::ReparentNodeCommand(Editor* editor, u64 node_id,
                                         u64 old_parent_id, u64 new_parent_id)
    : m_editor(editor)
    , m_node_id(node_id)
    , m_old_parent_id(old_parent_id)
    , m_new_parent_id(new_parent_id)
{
}

void ReparentNodeCommand::Execute()
{
    m_editor->SetNodeParent(m_node_id, m_new_parent_id);
}

void ReparentNodeCommand::Undo()
{
    m_editor->SetNodeParent(m_node_id, m_old_parent_id);
}

std::string ReparentNodeCommand::GetDescription() const
{
    return "Reparent Node";
}

} // namespace action
