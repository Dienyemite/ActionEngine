#include "history_panel.h"
#include <imgui/imgui.h>

namespace action {

void HistoryPanel::Draw(CommandHistory& history) {
    if (!visible) return;

    if (ImGui::Begin("History", &visible)) {
        const size_t undo_count = history.GetUndoCount();
        const size_t redo_count = history.GetRedoCount();

        // Toolbar
        bool can_undo = history.CanUndo();
        bool can_redo = history.CanRedo();

        if (!can_undo) ImGui::BeginDisabled();
        if (ImGui::Button("Undo")) { history.Undo(); }
        if (!can_undo) ImGui::EndDisabled();

        ImGui::SameLine();

        if (!can_redo) ImGui::BeginDisabled();
        if (ImGui::Button("Redo")) { history.Redo(); }
        if (!can_redo) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Clear")) { history.Clear(); }

        ImGui::Separator();

        // Show saved indicator
        if (history.IsDirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "* Unsaved Changes");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Saved");
        }

        ImGui::Separator();

        // History list heading
        ImGui::TextDisabled("Undo Stack (%zu actions)", undo_count);

        if (ImGui::BeginChild("##HistoryList", ImVec2(0, 0), false)) {
            // Redo entries (greyed out – already undone)
            if (redo_count > 0) {
                ImGui::TextDisabled("-- Current Position --");
                ImGui::Separator();
            }

            // Undo entries (most recent at top)
            for (int i = static_cast<int>(undo_count) - 1; i >= 0; --i) {
                bool is_top = (i == static_cast<int>(undo_count) - 1);

                if (is_top) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                }

                // We only have access to the top entry descriptions, so label by index
                char label[64];
                snprintf(label, sizeof(label), "%3d.", i + 1);
                ImGui::TextDisabled("%s", label);
                ImGui::SameLine();

                if (is_top) {
                    ImGui::Text("  %s", history.GetUndoDescription().c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextDisabled("  (action %d)", i + 1);
                }
            }

            // Show "Initial State" at the bottom
            ImGui::Separator();
            ImGui::TextDisabled("  (initial state)");
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace action
