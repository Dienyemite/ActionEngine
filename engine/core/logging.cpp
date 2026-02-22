#include "logging.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace action {

Logger::Logger() {
#ifdef PLATFORM_WINDOWS
    // Enable ANSI colors on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

void Logger::SetOutputFile(const std::string& path) {
    std::lock_guard lock(m_mutex);
    if (m_file.is_open()) {
        m_file.close();
    }
    m_file.open(path, std::ios::out | std::ios::trunc);
}

void Logger::LogInternal(LogLevel level, std::source_location loc, const std::string& message) {
    std::lock_guard lock(m_mutex);
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef PLATFORM_WINDOWS
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    // Extract filename from path
    std::string_view file = loc.file_name();
    auto last_slash = file.find_last_of("/\\");
    if (last_slash != std::string_view::npos) {
        file = file.substr(last_slash + 1);
    }
    
    // Format: [HH:MM:SS.mmm] [LEVEL] [file:line] message
    char timestamp[32];
    std::snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03d",
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());
    
    // Console output with colors
    if (m_colors_enabled.load(std::memory_order_relaxed)) {
        std::cout << "\033[90m[" << timestamp << "]\033[0m "
                  << LevelToColor(level) << "[" << LevelToString(level) << "]\033[0m "
                  << "\033[90m[" << file << ":" << loc.line() << "]\033[0m "
                  << message << "\n";
    } else {
        std::cout << "[" << timestamp << "] "
                  << "[" << LevelToString(level) << "] "
                  << "[" << file << ":" << loc.line() << "] "
                  << message << "\n";
    }
    
    // File output (no colors)
    if (m_file.is_open()) {
        m_file << "[" << timestamp << "] "
               << "[" << LevelToString(level) << "] "
               << "[" << file << ":" << loc.line() << "] "
               << message << "\n";
        // Flush only on Warn and above to avoid expensive I/O on every log line (#27)
        if (level >= LogLevel::Warn) {
            m_file.flush();
        }
    }
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "?????";
    }
}

const char* Logger::LevelToColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "\033[90m";    // Dark gray
        case LogLevel::Debug: return "\033[36m";    // Cyan
        case LogLevel::Info:  return "\033[32m";    // Green
        case LogLevel::Warn:  return "\033[33m";    // Yellow
        case LogLevel::Error: return "\033[31m";    // Red
        case LogLevel::Fatal: return "\033[35m";    // Magenta
        default: return "\033[0m";
    }
}

} // namespace action
