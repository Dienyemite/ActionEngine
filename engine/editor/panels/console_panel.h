#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <deque>

namespace action {

/*
 * ConsolePanel - Log output and command input
 * 
 * Features:
 * - Color-coded log messages (info, warning, error)
 * - Scrollable history
 * - Clear button
 * - Filter by log level
 * - Command input (future)
 */

class ConsolePanel {
public:
    ConsolePanel() = default;
    ~ConsolePanel() = default;
    
    void Draw();
    
    // Add message: level 0=info, 1=warning, 2=error
    void AddMessage(const std::string& message, int level = 0);
    void Clear();
    
    bool visible = true;
    
private:
    struct LogMessage {
        std::string text;
        int level;  // 0=info, 1=warning, 2=error
        u32 count;  // For collapsing repeated messages
    };
    
    std::deque<LogMessage> m_messages;
    static constexpr size_t MAX_MESSAGES = 1000;
    
    bool m_auto_scroll = true;
    bool m_show_info = true;
    bool m_show_warnings = true;
    bool m_show_errors = true;
    
    char m_filter_text[128] = "";
    char m_command_input[256] = "";
};

} // namespace action
