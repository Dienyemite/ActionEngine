#pragma once

#include "types.h"
#include <format>
#include <source_location>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>

namespace action {

enum class LogLevel : u8 {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5
};

class Logger {
public:
    static Logger& Get() {
        static Logger instance;
        return instance;
    }
    
    void SetLevel(LogLevel level) { m_min_level = level; }
    void SetOutputFile(const std::string& path);
    void EnableConsoleColors(bool enable) { m_colors_enabled = enable; }
    
    template<typename... Args>
    void Log(LogLevel level, std::source_location loc, std::format_string<Args...> fmt, Args&&... args) {
        if (level < m_min_level) return;
        
        std::string message = std::format(fmt, std::forward<Args>(args)...);
        LogInternal(level, loc, message);
    }
    
private:
    Logger();
    void LogInternal(LogLevel level, std::source_location loc, const std::string& message);
    
    const char* LevelToString(LogLevel level);
    const char* LevelToColor(LogLevel level);
    
    LogLevel m_min_level = LogLevel::Info;
    bool m_colors_enabled = true;
    std::mutex m_mutex;
    std::ofstream m_file;
};

// Convenience macros
#define LOG_TRACE(...) ::action::Logger::Get().Log(::action::LogLevel::Trace, std::source_location::current(), __VA_ARGS__)
#define LOG_DEBUG(...) ::action::Logger::Get().Log(::action::LogLevel::Debug, std::source_location::current(), __VA_ARGS__)
#define LOG_INFO(...)  ::action::Logger::Get().Log(::action::LogLevel::Info,  std::source_location::current(), __VA_ARGS__)
#define LOG_WARN(...)  ::action::Logger::Get().Log(::action::LogLevel::Warn,  std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...) ::action::Logger::Get().Log(::action::LogLevel::Error, std::source_location::current(), __VA_ARGS__)
#define LOG_FATAL(...) ::action::Logger::Get().Log(::action::LogLevel::Fatal, std::source_location::current(), __VA_ARGS__)

// Assertions
#define ENGINE_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            LOG_FATAL("Assertion failed: " #condition); \
            LOG_FATAL(__VA_ARGS__); \
            std::abort(); \
        } \
    } while (0)

#ifdef _DEBUG
#define ENGINE_DEBUG_ASSERT(condition, ...) ENGINE_ASSERT(condition, __VA_ARGS__)
#else
#define ENGINE_DEBUG_ASSERT(condition, ...) ((void)0)
#endif

} // namespace action
