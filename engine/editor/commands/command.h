#pragma once

#include "core/types.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace action {

/*
 * Command - Base class for undoable/redoable actions
 * 
 * Implements the Command pattern for editor operations.
 * Each command knows how to execute itself and undo itself.
 */
class Command {
public:
    virtual ~Command() = default;
    
    // Execute the command (first time or redo)
    virtual void Execute() = 0;
    
    // Undo the command
    virtual void Undo() = 0;
    
    // Get description for UI display
    virtual std::string GetDescription() const = 0;
    
    // Can this command be merged with another? (for continuous operations like dragging)
    virtual bool CanMergeWith(const Command* other) const { return false; }
    
    // Merge another command into this one
    virtual void MergeWith(const Command* other) {}
    
    // Get command type ID for merging
    virtual u32 GetTypeId() const { return 0; }
};

/*
 * CommandHistory - Manages undo/redo stacks
 */
class CommandHistory {
public:
    CommandHistory(size_t max_history = 100);
    ~CommandHistory() = default;
    
    // Execute a command and add it to history
    void Execute(std::unique_ptr<Command> command);
    
    // Add a command to history without executing it (for already-applied changes like gizmo drags)
    void AddWithoutExecute(std::unique_ptr<Command> command);
    
    // Undo the last command
    bool Undo();
    
    // Redo the last undone command
    bool Redo();
    
    // Check if undo/redo are available
    bool CanUndo() const { return !m_undo_stack.empty(); }
    bool CanRedo() const { return !m_redo_stack.empty(); }
    
    // Get descriptions for UI
    std::string GetUndoDescription() const;
    std::string GetRedoDescription() const;
    
    // Clear all history
    void Clear();
    
    // Get history size
    size_t GetUndoCount() const { return m_undo_stack.size(); }
    size_t GetRedoCount() const { return m_redo_stack.size(); }
    
    // Mark current state as saved (clears "dirty" flag logic)
    void MarkSaved() { m_saved_index = m_undo_stack.size(); }
    bool IsDirty() const { return m_undo_stack.size() != m_saved_index; }
    
private:
    std::vector<std::unique_ptr<Command>> m_undo_stack;
    std::vector<std::unique_ptr<Command>> m_redo_stack;
    size_t m_max_history;
    size_t m_saved_index = 0;
};

} // namespace action
