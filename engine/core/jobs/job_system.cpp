#include "job_system.h"
#include "../logging.h"
#include "../profiler.h"
#include <immintrin.h>  // For _mm_pause()
#include <chrono>

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace action {

void JobHandle::Wait() const {
    if (counter) {
        // Use exponential backoff to reduce CPU waste
        u32 spin_count = 0;
        while (counter->load(std::memory_order_acquire) > 0) {
            if (spin_count < 16) {
                // Brief spin for fast completion
                _mm_pause();  // CPU hint for spin-wait
            } else if (spin_count < 64) {
                std::this_thread::yield();
            } else {
                // Longer sleep for truly long waits
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            ++spin_count;
        }
    }
}

bool JobSystem::Initialize(u32 worker_count) {
    m_main_thread_id = std::this_thread::get_id();
    m_worker_count = worker_count;
    m_running = true;
    
    // Initialize counters
    for (auto& counter : m_counters) {
        counter.store(0);
    }
    
    LOG_INFO("JobSystem: Starting {} worker threads", worker_count);
    
    // Create worker threads
    m_workers.reserve(worker_count);
    for (u32 i = 0; i < worker_count; ++i) {
        m_workers.emplace_back(&JobSystem::WorkerThread, this, i + 1);
        
#ifdef PLATFORM_WINDOWS
        // Set thread affinity for cache locality
        HANDLE handle = m_workers.back().native_handle();
        SetThreadAffinityMask(handle, 1ULL << (i + 1));
        
        // Set thread name for debugging
        std::wstring name = L"Worker " + std::to_wstring(i + 1);
        SetThreadDescription(handle, name.c_str());
#endif
    }
    
    return true;
}

void JobSystem::Shutdown() {
    LOG_INFO("JobSystem: Shutting down");
    
    m_running = false;
    
    // Wake up all workers
    for (auto& queue : m_queues) {
        queue.cv.notify_all();
    }
    
    // Join all workers
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    m_workers.clear();
}

JobHandle JobSystem::Submit(JobFunction func, JobPriority priority) {
    Job job{
        .function = std::move(func),
        .priority = priority,
        .counter = AllocateCounter()
    };
    
    job.counter->store(1);
    JobHandle handle{job.counter};
    
    auto& queue = m_queues[static_cast<u32>(priority)];
    {
        std::lock_guard lock(queue.mutex);
        queue.jobs.push_back(std::move(job));
    }
    
    m_pending_jobs++;
    queue.cv.notify_one();
    
    return handle;
}

JobHandle JobSystem::SubmitBatch(std::span<Job> jobs) {
    if (jobs.empty()) return {};
    
    auto* counter = AllocateCounter();
    counter->store(static_cast<u32>(jobs.size()));
    JobHandle handle{counter};
    
    // Group by priority and submit
    for (auto& job : jobs) {
        job.counter = counter;
        
        auto& queue = m_queues[static_cast<u32>(job.priority)];
        {
            std::lock_guard lock(queue.mutex);
            queue.jobs.push_back(std::move(job));
        }
        queue.cv.notify_one();
    }
    
    m_pending_jobs += static_cast<u32>(jobs.size());
    return handle;
}

JobHandle JobSystem::ParallelFor(u32 count, std::function<void(u32, u32)> func, 
                                  u32 batch_size, JobPriority priority) {
    if (count == 0) return {};
    
    u32 batch_count = (count + batch_size - 1) / batch_size;
    auto* counter = AllocateCounter();
    counter->store(batch_count);
    JobHandle handle{counter};
    
    auto& queue = m_queues[static_cast<u32>(priority)];
    
    for (u32 batch = 0; batch < batch_count; ++batch) {
        u32 start = batch * batch_size;
        u32 end = std::min(start + batch_size, count);
        
        Job job{
            .function = [func, start, end]() {
                u32 tid = 0; // Will be set properly in worker
                for (u32 i = start; i < end; ++i) {
                    func(i, tid);
                }
            },
            .priority = priority,
            .counter = counter,
            .batch_index = batch,
            .batch_count = batch_count
        };
        
        {
            std::lock_guard lock(queue.mutex);
            queue.jobs.push_back(std::move(job));
        }
    }
    
    m_pending_jobs += batch_count;
    queue.cv.notify_all();
    
    return handle;
}

void JobSystem::SubmitMainThread(JobFunction func) {
    std::lock_guard lock(m_main_queue_mutex);
    m_main_thread_queue.push(std::move(func));
}

void JobSystem::ExecuteMainThreadJobs() {
    PROFILE_SCOPE("MainThreadJobs");
    
    std::queue<JobFunction> jobs;
    {
        std::lock_guard lock(m_main_queue_mutex);
        std::swap(jobs, m_main_thread_queue);
    }
    
    while (!jobs.empty()) {
        jobs.front()();
        jobs.pop();
    }
}

void JobSystem::Wait(const JobHandle& handle) {
    if (handle.IsComplete()) return;
    
    // Busy-wait while trying to help with work
    while (!handle.IsComplete()) {
        if (!TryExecuteJob()) {
            std::this_thread::yield();
        }
    }
}

void JobSystem::WaitAll() {
    while (m_pending_jobs.load() > 0) {
        if (!TryExecuteJob()) {
            std::this_thread::yield();
        }
    }
}

u32 JobSystem::GetCurrentThreadId() const {
    auto id = std::this_thread::get_id();
    if (id == m_main_thread_id) return 0;
    
    for (u32 i = 0; i < m_workers.size(); ++i) {
        if (m_workers[i].get_id() == id) {
            return i + 1;
        }
    }
    return UINT32_MAX;
}

bool JobSystem::IsMainThread() const {
    return std::this_thread::get_id() == m_main_thread_id;
}

void JobSystem::WorkerThread(u32 thread_id) {
    LOG_DEBUG("Worker thread {} started", thread_id);
    
    while (m_running) {
        if (!TryExecuteJob()) {
            // Wait for work
            auto& queue = m_queues[static_cast<u32>(JobPriority::Normal)];
            std::unique_lock lock(queue.mutex);
            queue.cv.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                return !m_running || m_pending_jobs.load() > 0;
            });
        }
    }
    
    LOG_DEBUG("Worker thread {} exiting", thread_id);
}

bool JobSystem::TryExecuteJob() {
    // Try to get a job, starting from highest priority
    for (int p = 3; p >= 0; --p) {
        auto& queue = m_queues[p];
        
        Job job;
        {
            std::lock_guard lock(queue.mutex);
            if (queue.jobs.empty()) continue;
            
            job = std::move(queue.jobs.front());
            queue.jobs.pop_front();
        }
        
        // Execute the job
        if (job.function) {
            job.function();
        }
        
        // Decrement counter
        if (job.counter) {
            job.counter->fetch_sub(1, std::memory_order_release);
        }
        
        m_pending_jobs--;
        return true;
    }
    
    return false;
}

std::atomic<u32>* JobSystem::AllocateCounter() {
    u32 index = m_counter_index.fetch_add(1) % MAX_COUNTERS;
    return &m_counters[index];
}

void JobSystem::FreeCounter(std::atomic<u32>* counter) {
    // Counters are reused via circular buffer, no explicit free needed
    (void)counter;
}

} // namespace action
