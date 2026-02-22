#pragma once

#include "types.h"
#include <format>
#include <source_location>
#include <iostream>
#include <fstream>
#include <mutex>
#include <atomic>
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
    
    void SetLevel(LogLevel level) { m_min_level.store(level, std::memory_order_relaxed); }
    void SetOutputFile(const std::string& path);
    void EnableConsoleColors(bool enable) { m_colors_enabled.store(enable, std::memory_order_relaxed); }
    
    template<typename... Args>
    void Log(LogLevel level, std::source_location loc, std::format_string<Args...> fmt, Args&&... args) {
        if (level < m_min_level.load(std::memory_order_relaxed)) return;
        
        std::string message = std::format(fmt, std::forward<Args>(args)...);
        LogInternal(level, loc, message);
    }
    
private:
    Logger();
    void LogInternal(LogLevel level, std::source_location loc, const std::string& message);
    
    const char* LevelToString(LogLevel level);
    const char* LevelToColor(LogLevel level);
    
    std::atomic<LogLevel> m_min_level{LogLevel::Info};   // #28: atomic to prevent data race
    std::atomic<bool>     m_colors_enabled{true};        // #28: atomic to prevent data race
    std::mutex m_mutex;
    std::ofstream m_file;
};

// ===== Assertion helper (#29) =========================================================
// Indirection through a template function avoids __VA_OPT__ + semicolon MSVC bug and
// also eliminates the LOG_FATAL() zero-arg call UB that existed in the original code.
namespace detail {
    // Zero-extra-args overload: logs just the condition string.
    inline void assert_fail(const char* cond, std::source_location loc) {
        Logger::Get().Log(LogLevel::Fatal, loc, "Assertion failed: {}", cond);
        std::abort();
    }
    // N-extra-args overload: forwards the caller-provided format+args.
    // std::format_string<Args...> matches string literals at the call site (consteval).
    template<typename... Args>
    void assert_fail(const char* cond, std::source_location loc,
                     std::format_string<Args...> fmt, Args&&... args) {
        Logger::Get().Log(LogLevel::Fatal, loc, "Assertion failed: {}", cond);
        Logger::Get().Log(LogLevel::Fatal, loc, fmt, std::forward<Args>(args)...);
        std::abort();
    }
} // namespace detail

// Convenience macros
#define LOG_TRACE(...) ::action::Logger::Get().Log(::action::LogLevel::Trace, std::source_location::current(), __VA_ARGS__)
#define LOG_DEBUG(...) ::action::Logger::Get().Log(::action::LogLevel::Debug, std::source_location::current(), __VA_ARGS__)
#define LOG_INFO(...)  ::action::Logger::Get().Log(::action::LogLevel::Info,  std::source_location::current(), __VA_ARGS__)
#define LOG_WARN(...)  ::action::Logger::Get().Log(::action::LogLevel::Warn,  std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...) ::action::Logger::Get().Log(::action::LogLevel::Error, std::source_location::current(), __VA_ARGS__)
#define LOG_FATAL(...) ::action::Logger::Get().Log(::action::LogLevel::Fatal, std::source_location::current(), __VA_ARGS__)

// Assertions
// ENGINE_ASSERT(condition)            -- no message variant
// ENGINE_ASSERT(condition, fmt, ...)  -- message + format variant
// Uses detail::assert_fail to avoid MSVC __VA_OPT__+semicolon bug (#29).
// __VA_OPT__(,) inserts a comma only when extra args are present; no semicolon
// inside __VA_OPT__() so the MSVC C2059 bug is not triggered.
#define ENGINE_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            ::action::detail::assert_fail(#condition, std::source_location::current() __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while (0)

#ifdef _DEBUG
#define ENGINE_DEBUG_ASSERT(condition, ...) ENGINE_ASSERT(condition, __VA_ARGS__)
#else
#define ENGINE_DEBUG_ASSERT(condition, ...) ((void)0)
#endif

} // namespace action
