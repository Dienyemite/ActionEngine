#include "profiler.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace action {

thread_local std::vector<u32> Profiler::t_scope_stack;
thread_local u32 Profiler::t_current_depth = 0;

Profiler::Profiler() {
#ifdef PLATFORM_WINDOWS
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_frequency = freq.QuadPart;
#else
    m_frequency = 1000000000; // nanoseconds
#endif
}

u64 Profiler::GetTimestamp() {
#ifdef PLATFORM_WINDOWS
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
#else
    auto now = std::chrono::high_resolution_clock::now();
    return now.time_since_epoch().count();
#endif
}

u32 Profiler::GetThreadId() {
#ifdef PLATFORM_WINDOWS
    return GetCurrentThreadId();
#else
    return static_cast<u32>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

void Profiler::BeginFrame() {
    std::lock_guard lock(m_mutex);
    m_last_frame_samples = std::move(m_current_samples);
    m_current_samples.clear();
    t_scope_stack.clear();
    t_current_depth = 0;
}

void Profiler::EndFrame() {
    // Nothing special needed
}

void Profiler::BeginScope(const char* name) {
    if (!m_enabled) return;
    
    ProfileSample sample;
    sample.name = name;
    sample.start_time = GetTimestamp();
    sample.depth = t_current_depth++;
    sample.thread_id = GetThreadId();
    
    std::lock_guard lock(m_mutex);
    t_scope_stack.push_back(static_cast<u32>(m_current_samples.size()));
    m_current_samples.push_back(sample);
}

void Profiler::EndScope() {
    if (!m_enabled) return;
    
    u64 end_time = GetTimestamp();
    t_current_depth--;
    
    std::lock_guard lock(m_mutex);
    if (!t_scope_stack.empty()) {
        u32 sample_index = t_scope_stack.back();
        t_scope_stack.pop_back();
        
        if (sample_index < m_current_samples.size()) {
            m_current_samples[sample_index].end_time = end_time;
            
            // Update aggregate stats
            auto& sample = m_current_samples[sample_index];
            float duration_ms = static_cast<float>(sample.end_time - sample.start_time) 
                              * 1000.0f / m_frequency;
            
            auto& stats = m_aggregate_stats[sample.name];
            stats.name = sample.name;
            stats.call_count++;
            stats.avg_ms = stats.avg_ms * 0.95f + duration_ms * 0.05f; // Rolling average
            stats.min_ms = std::min(stats.min_ms, duration_ms);
            stats.max_ms = std::max(stats.max_ms, duration_ms);
        }
    }
}

std::vector<Profiler::ScopeStats> Profiler::GetAggregateStats() const {
    std::lock_guard lock(const_cast<std::mutex&>(m_mutex));
    
    std::vector<ScopeStats> result;
    result.reserve(m_aggregate_stats.size());
    
    for (const auto& [name, stats] : m_aggregate_stats) {
        result.push_back(stats);
    }
    
    return result;
}

} // namespace action
