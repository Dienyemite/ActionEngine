#pragma once

#include "types.h"
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace action {

// Lightweight profiler for CPU timing
// For GPU timing, see Renderer's GPU timestamp queries

struct ProfileSample {
    std::string name;
    u64 start_time;
    u64 end_time;
    u32 depth;
    u32 thread_id;
};

class Profiler {
public:
    static Profiler& Get() {
        static Profiler instance;
        return instance;
    }
    
    void BeginFrame();
    void EndFrame();
    
    void BeginScope(const char* name);
    void EndScope();
    
    // Get last frame's samples
    const std::vector<ProfileSample>& GetSamples() const { return m_last_frame_samples; }
    
    // Aggregate stats
    struct ScopeStats {
        std::string name;
        float avg_ms;
        float min_ms;
        float max_ms;
        u32 call_count;
    };
    std::vector<ScopeStats> GetAggregateStats() const;
    
    // Enable/disable
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    
private:
    Profiler();
    
    u64 GetTimestamp();
    u32 GetThreadId();
    
    bool m_enabled = true;
    u64 m_frequency;
    
    std::mutex m_mutex;
    std::vector<ProfileSample> m_current_samples;
    std::vector<ProfileSample> m_last_frame_samples;
    
    // Per-thread scope stack
    thread_local static std::vector<u32> t_scope_stack;
    thread_local static u32 t_current_depth;
    
    // Aggregate data (rolling average)
    std::unordered_map<std::string, ScopeStats> m_aggregate_stats;
};

// RAII scope helper
class ProfileScope {
public:
    ProfileScope(const char* name) {
        Profiler::Get().BeginScope(name);
    }
    ~ProfileScope() {
        Profiler::Get().EndScope();
    }
    
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

#if ENGINE_ENABLE_PROFILING
    #define PROFILE_SCOPE(name) ::action::ProfileScope _profile_scope_##__LINE__(name)
    #define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#else
    #define PROFILE_SCOPE(name) ((void)0)
    #define PROFILE_FUNCTION() ((void)0)
#endif

} // namespace action
