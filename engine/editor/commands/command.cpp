#include "command.h"

namespace action {

CommandHistory::CommandHistory(size_t max_history)
    : m_max_history(max_history)
{
}

void CommandHistory::Execute(std::unique_ptr<Command> command)
{
    if (!command) return;
    
    // Check if we can merge with the last command
    if (!m_undo_stack.empty()) {
        Command* last = m_undo_stack.back().get();
        if (last->CanMergeWith(command.get())) {
            last->MergeWith(command.get());
            command->Execute();
            // Clear redo stack on new action
            m_redo_stack.clear();
            return;
        }
    }
    
    // Execute the command
    command->Execute();
    
    // Add to undo stack
    m_undo_stack.push_back(std::move(command));
    
    // Clear redo stack (new action invalidates redo history)
    m_redo_stack.clear();
    
    // Limit history size
    while (m_undo_stack.size() > m_max_history) {
        m_undo_stack.erase(m_undo_stack.begin());
        if (m_saved_index > 0) m_saved_index--;
    }
}

void CommandHistory::AddWithoutExecute(std::unique_ptr<Command> command)
{
    if (!command) return;
    
    // Check if we can merge with the last command
    if (!m_undo_stack.empty()) {
        Command* last = m_undo_stack.back().get();
        if (last->CanMergeWith(command.get())) {
            last->MergeWith(command.get());
            // Clear redo stack on new action
            m_redo_stack.clear();
            return;
        }
    }
    
    // DON'T execute - just add to history
    m_undo_stack.push_back(std::move(command));
    
    // Clear redo stack
    m_redo_stack.clear();
    
    // Limit history size
    while (m_undo_stack.size() > m_max_history) {
        m_undo_stack.erase(m_undo_stack.begin());
        if (m_saved_index > 0) m_saved_index--;
    }
}

bool CommandHistory::Undo()
{
    if (m_undo_stack.empty()) return false;
    
    // Pop from undo stack
    std::unique_ptr<Command> command = std::move(m_undo_stack.back());
    m_undo_stack.pop_back();
    
    // Undo it
    command->Undo();
    
    // Push to redo stack
    m_redo_stack.push_back(std::move(command));
    
    return true;
}

bool CommandHistory::Redo()
{
    if (m_redo_stack.empty()) return false;
    
    // Pop from redo stack
    std::unique_ptr<Command> command = std::move(m_redo_stack.back());
    m_redo_stack.pop_back();
    
    // Execute again
    command->Execute();
    
    // Push back to undo stack
    m_undo_stack.push_back(std::move(command));
    
    return true;
}

std::string CommandHistory::GetUndoDescription() const
{
    if (m_undo_stack.empty()) return "";
    return m_undo_stack.back()->GetDescription();
}

std::string CommandHistory::GetRedoDescription() const
{
    if (m_redo_stack.empty()) return "";
    return m_redo_stack.back()->GetDescription();
}

void CommandHistory::Clear()
{
    m_undo_stack.clear();
    m_redo_stack.clear();
    m_saved_index = 0;
}

} // namespace action
