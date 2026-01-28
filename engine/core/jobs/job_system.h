#pragma once

#include "../types.h"
#include "../memory/allocators.h"
#include <atomic>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace action {

/*
 * Job System for i5-6600K (4 cores / 4 threads)
 * 
 * Design:
 * - Simple thread pool (no fibers for simplicity)
 * - Coarse-grained jobs to minimize overhead
 * - Priority levels for critical path
 * - Main thread job queue for GPU submissions
 * 
 * Thread layout:
 * - Thread 0: Main (game logic, input, render submit)
 * - Thread 1-3: Workers (physics, streaming, world update)
 */

// Forward declarations
class JobSystem;

// Job priority levels
enum class JobPriority : u8 {
    Low = 0,      // Background tasks (asset compression, etc.)
    Normal = 1,   // Standard work
    High = 2,     // Time-sensitive (streaming, LOD)
    Critical = 3  // Must complete this frame
};

// Job handle for synchronization
struct JobHandle {
    std::atomic<u32>* counter = nullptr;
    
    bool IsComplete() const { 
        return counter == nullptr || counter->load(std::memory_order_acquire) == 0; 
    }
    void Wait() const;
};

// Job function signature
using JobFunction = std::function<void()>;

// Job definition
struct Job {
    JobFunction function;
    JobPriority priority = JobPriority::Normal;
    std::atomic<u32>* counter = nullptr;
    
    // For splitting large workloads
    u32 batch_index = 0;
    u32 batch_count = 1;
};

// Parallel-for helper
struct ParallelForJob {
    std::function<void(u32 index, u32 thread_id)> function;
    u32 count = 0;
    u32 batch_size = 64; // Items per job
};

class JobSystem {
public:
    JobSystem() = default;
    ~JobSystem() = default;
    
    // Initialize with worker count (excluding main thread)
    bool Initialize(u32 worker_count = 3);
    void Shutdown();
    
    // Submit a single job
    JobHandle Submit(JobFunction func, JobPriority priority = JobPriority::Normal);
    
    // Submit batch of jobs with shared counter
    JobHandle SubmitBatch(std::span<Job> jobs);
    
    // Parallel for - splits work across threads
    // Example: ParallelFor(1000, [](u32 i, u32 tid) { ... });
    JobHandle ParallelFor(u32 count, std::function<void(u32, u32)> func, 
                          u32 batch_size = 64, JobPriority priority = JobPriority::Normal);
    
    // Submit job to run on main thread (for GPU submissions, etc.)
    void SubmitMainThread(JobFunction func);
    
    // Called from main thread to execute pending main-thread jobs
    void ExecuteMainThreadJobs();
    
    // Wait for job to complete (busy-wait with work stealing)
    void Wait(const JobHandle& handle);
    
    // Wait for all jobs to complete
    void WaitAll();
    
    // Stats
    u32 GetWorkerCount() const { return m_worker_count; }
    u32 GetPendingJobCount() const { return m_pending_jobs.load(); }
    u32 GetCurrentThreadId() const;
    bool IsMainThread() const;
    
private:
    void WorkerThread(u32 thread_id);
    bool TryExecuteJob();
    Job* StealJob();
    
    // Workers
    std::vector<std::thread> m_workers;
    u32 m_worker_count = 0;
    std::atomic<bool> m_running{false};
    
    // Main thread ID
    std::thread::id m_main_thread_id;
    
    // Job queues (one per priority level)
    struct JobQueue {
        std::mutex mutex;
        std::deque<Job> jobs;
        std::condition_variable cv;
    };
    std::array<JobQueue, 4> m_queues; // One per priority level
    
    // Main thread job queue
    std::mutex m_main_queue_mutex;
    std::queue<JobFunction> m_main_thread_queue;
    
    // Counter pool for job handles
    static constexpr size_t MAX_COUNTERS = 256;
    std::array<std::atomic<u32>, MAX_COUNTERS> m_counters;
    std::atomic<u32> m_counter_index{0};
    
    std::atomic<u32> m_pending_jobs{0};
    
    std::atomic<u32>* AllocateCounter();
    void FreeCounter(std::atomic<u32>* counter);
};

// Scoped job wait helper
class ScopedJobWait {
public:
    ScopedJobWait(JobSystem& system, JobHandle handle) 
        : m_system(system), m_handle(handle) {}
    ~ScopedJobWait() { m_system.Wait(m_handle); }
    
private:
    JobSystem& m_system;
    JobHandle m_handle;
};

} // namespace action
