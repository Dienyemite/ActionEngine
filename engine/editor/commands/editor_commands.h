#pragma once

#include "command.h"
#include "core/math/math.h"
#include <string>

namespace action {

// Forward declarations
class Editor;

/*
 * TransformCommand - Undoable transform change
 * 
 * Records old and new transform values for a node.
 * Supports merging for continuous drag operations.
 */
class TransformCommand : public Command {
public:
    enum class Type { Position, Rotation, Scale };
    
    TransformCommand(Editor* editor, u64 node_id, Type type,
                     const vec3& old_value, const vec3& new_value);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;
    
    bool CanMergeWith(const Command* other) const override;
    void MergeWith(const Command* other) override;
    u32 GetTypeId() const override { return 1; }
    
private:
    Editor* m_editor;
    u64 m_node_id;
    Type m_type;
    vec3 m_old_value;
    vec3 m_new_value;
};

/*
 * AddNodeCommand - Undoable node creation
 */
class AddNodeCommand : public Command {
public:
    AddNodeCommand(Editor* editor, const std::string& name, u64 parent_id = 0);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;
    u32 GetTypeId() const override { return 2; }
    
    u64 GetCreatedNodeId() const { return m_created_id; }
    
private:
    Editor* m_editor;
    std::string m_name;
    u64 m_parent_id;
    u64 m_created_id = 0;
    
    // Stored data for redo
    vec3 m_position;
    vec3 m_rotation;
    vec3 m_scale;
    vec3 m_color;
};

/*
 * DeleteNodeCommand - Undoable node deletion
 */
class DeleteNodeCommand : public Command {
public:
    DeleteNodeCommand(Editor* editor, u64 node_id);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;
    u32 GetTypeId() const override { return 3; }
    
private:
    Editor* m_editor;
    u64 m_node_id;
    
    // Stored data for undo (restore the node)
    std::string m_name;
    u64 m_parent_id;
    vec3 m_position;
    vec3 m_rotation;
    vec3 m_scale;
    vec3 m_color;
};

/*
 * RenameNodeCommand - Undoable node rename
 */
class RenameNodeCommand : public Command {
public:
    RenameNodeCommand(Editor* editor, u64 node_id, 
                      const std::string& old_name, const std::string& new_name);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;
    u32 GetTypeId() const override { return 4; }
    
private:
    Editor* m_editor;
    u64 m_node_id;
    std::string m_old_name;
    std::string m_new_name;
};

/*
 * ColorChangeCommand - Undoable color change
 */
class ColorChangeCommand : public Command {
public:
    ColorChangeCommand(Editor* editor, u64 node_id,
                       const vec3& old_color, const vec3& new_color);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;
    
    bool CanMergeWith(const Command* other) const override;
    void MergeWith(const Command* other) override;
    u32 GetTypeId() const override { return 5; }
    
private:
    Editor* m_editor;
    u64 m_node_id;
    vec3 m_old_color;
    vec3 m_new_color;
};

/*
 * ReparentNodeCommand - Undoable node hierarchy change
 */
class ReparentNodeCommand : public Command {
public:
    ReparentNodeCommand(Editor* editor, u64 node_id, 
                        u64 old_parent_id, u64 new_parent_id);
    
    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;
    u32 GetTypeId() const override { return 6; }
    
private:
    Editor* m_editor;
    u64 m_node_id;
    u64 m_old_parent_id;
    u64 m_new_parent_id;
};

} // namespace action
