#include "console_panel.h"
#include <imgui/imgui.h>
#include <cstring>

namespace action {

void ConsolePanel::Draw() {
    if (!visible) return;
    
    if (ImGui::Begin("Console", &visible)) {
        // Toolbar
        if (ImGui::Button("Clear")) {
            Clear();
        }
        
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_auto_scroll);
        
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        
        // Level filters with colored buttons
        ImGui::PushStyleColor(ImGuiCol_Button, m_show_info ? ImVec4(0.3f, 0.5f, 0.7f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Info")) m_show_info = !m_show_info;
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, m_show_warnings ? ImVec4(0.7f, 0.6f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Warn")) m_show_warnings = !m_show_warnings;
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, m_show_errors ? ImVec4(0.7f, 0.3f, 0.3f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Error")) m_show_errors = !m_show_errors;
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        
        // Filter text
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##filter", "Filter...", m_filter_text, sizeof(m_filter_text));
        
        ImGui::Separator();
        
        // Log output area
        float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollRegion", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            for (const auto& msg : m_messages) {
                // Filter by level
                if (msg.level == 0 && !m_show_info) continue;
                if (msg.level == 1 && !m_show_warnings) continue;
                if (msg.level == 2 && !m_show_errors) continue;
                
                // Filter by text
                if (m_filter_text[0] != '\0') {
                    if (msg.text.find(m_filter_text) == std::string::npos) {
                        continue;
                    }
                }
                
                // Color based on level
                ImVec4 color;
                const char* prefix;
                switch (msg.level) {
                    case 0:
                        color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                        prefix = "[INFO]";
                        break;
                    case 1:
                        color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
                        prefix = "[WARN]";
                        break;
                    case 2:
                        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                        prefix = "[ERROR]";
                        break;
                    default:
                        color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                        prefix = "[???]";
                        break;
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                
                if (msg.count > 1) {
                    ImGui::Text("%s (%u) %s", prefix, msg.count, msg.text.c_str());
                } else {
                    ImGui::Text("%s %s", prefix, msg.text.c_str());
                }
                
                ImGui::PopStyleColor();
            }
            
            // Auto-scroll to bottom
            if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        
        ImGui::Separator();
        
        // Command input
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##command", m_command_input, sizeof(m_command_input), 
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (m_command_input[0] != '\0') {
                // Echo command
                AddMessage(std::string("> ") + m_command_input, 0);
                
                // TODO: Execute command
                AddMessage("Command execution not implemented", 1);
                
                m_command_input[0] = '\0';
            }
            
            // Keep focus on input
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

void ConsolePanel::AddMessage(const std::string& message, int level) {
    // Check if we can collapse with previous message
    if (!m_messages.empty()) {
        auto& last = m_messages.back();
        if (last.text == message && last.level == level) {
            last.count++;
            return;
        }
    }
    
    // Add new message
    LogMessage msg;
    msg.text = message;
    msg.level = level;
    msg.count = 1;
    m_messages.push_back(msg);
    
    // Limit message count
    while (m_messages.size() > MAX_MESSAGES) {
        m_messages.pop_front();
    }
}

void ConsolePanel::Clear() {
    m_messages.clear();
}

} // namespace action
