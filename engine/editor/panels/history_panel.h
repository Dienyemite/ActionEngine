#pragma once

#include "editor/commands/command.h"
#include <string>

namespace action {

/*
 * HistoryPanel — Godot-style undo/redo history viewer.
 *
 * Displays the command history stack. Clicking an entry jumps to that point
 * in history (like Godot's History dock). Undo entries appear greyed out when
 * they are on the redo stack.
 */
class HistoryPanel {
public:
    HistoryPanel() = default;
    ~HistoryPanel() = default;

    // Draw the panel. Pass the editor's CommandHistory.
    void Draw(CommandHistory& history);

    bool visible = false;   // Hidden by default, toggle from View menu
};

} // namespace action
