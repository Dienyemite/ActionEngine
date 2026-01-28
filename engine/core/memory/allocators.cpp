#include "allocators.h"
#include "../logging.h"
#include <cstdlib>

#ifdef _MSC_VER
#include <malloc.h>
#define ALIGNED_ALLOC(alignment, size) _aligned_malloc(size, alignment)
#define ALIGNED_FREE(ptr) _aligned_free(ptr)
#else
#define ALIGNED_ALLOC(alignment, size) std::aligned_alloc(alignment, size)
#define ALIGNED_FREE(ptr) std::free(ptr)
#endif

namespace action {

// Linear Allocator
LinearAllocator::LinearAllocator(size_t capacity) 
    : m_capacity(capacity)
    , m_offset(0) 
{
    m_memory = static_cast<u8*>(ALIGNED_ALLOC(DEFAULT_ALIGNMENT, capacity));
    ENGINE_ASSERT(m_memory, "Failed to allocate linear allocator memory");
}

LinearAllocator::~LinearAllocator() {
    ALIGNED_FREE(m_memory);
}

void* LinearAllocator::Allocate(size_t size, size_t alignment) {
    size_t aligned_offset = AlignUp(m_offset, alignment);
    size_t new_offset = aligned_offset + size;
    
    if (new_offset > m_capacity) {
        LOG_ERROR("LinearAllocator out of memory: requested {} bytes, have {} remaining",
                  size, m_capacity - m_offset);
        return nullptr;
    }
    
    m_offset = new_offset;
    return m_memory + aligned_offset;
}

void LinearAllocator::Free(void* ptr) {
    // No-op - linear allocators don't support individual frees
    (void)ptr;
}

void LinearAllocator::Reset() {
    m_offset = 0;
}

// Stack Allocator
StackAllocator::StackAllocator(size_t capacity)
    : m_capacity(capacity)
    , m_offset(0)
{
    m_memory = static_cast<u8*>(ALIGNED_ALLOC(DEFAULT_ALIGNMENT, capacity));
    ENGINE_ASSERT(m_memory, "Failed to allocate stack allocator memory");
}

StackAllocator::~StackAllocator() {
    ALIGNED_FREE(m_memory);
}

void* StackAllocator::Allocate(size_t size, size_t alignment) {
    size_t header_size = sizeof(AllocationHeader);
    size_t total_size = header_size + size + alignment;
    
    if (m_offset + total_size > m_capacity) {
        LOG_ERROR("StackAllocator out of memory");
        return nullptr;
    }
    
    // Align the user data pointer
    size_t data_offset = AlignUp(m_offset + header_size, alignment);
    size_t adjustment = data_offset - m_offset - header_size;
    
    // Store header just before user data
    AllocationHeader* header = reinterpret_cast<AllocationHeader*>(m_memory + data_offset - header_size);
    header->prev_offset = m_offset;
    header->adjustment = adjustment;
    
    m_offset = data_offset + size;
    
    return m_memory + data_offset;
}

void StackAllocator::Free(void* ptr) {
    if (!ptr) return;
    
    // Read header
    u8* data_ptr = static_cast<u8*>(ptr);
    AllocationHeader* header = reinterpret_cast<AllocationHeader*>(data_ptr - sizeof(AllocationHeader));
    
    // Validate LIFO order
    ENGINE_DEBUG_ASSERT(data_ptr >= m_memory && data_ptr < m_memory + m_offset,
                        "Invalid pointer passed to StackAllocator::Free");
    
    m_offset = header->prev_offset;
}

void StackAllocator::Reset() {
    m_offset = 0;
}

// Heap Allocator
HeapAllocator::~HeapAllocator() {
    if (m_allocation_count > 0) {
        LOG_WARN("HeapAllocator destroyed with {} active allocations ({} bytes)",
                 m_allocation_count.load(), m_allocated_size.load());
    }
}

void* HeapAllocator::Allocate(size_t size, size_t alignment) {
    void* ptr = ALIGNED_ALLOC(alignment, AlignUp(size, alignment));
    
    if (ptr) {
        m_allocated_size += size;
        m_allocation_count++;
        
#ifdef _DEBUG
        std::lock_guard lock(m_debug_mutex);
        m_allocations[ptr] = size;
#endif
    }
    
    return ptr;
}

void HeapAllocator::Free(void* ptr) {
    if (!ptr) return;
    
#ifdef _DEBUG
    {
        std::lock_guard lock(m_debug_mutex);
        auto it = m_allocations.find(ptr);
        if (it != m_allocations.end()) {
            m_allocated_size -= it->second;
            m_allocation_count--;
            m_allocations.erase(it);
        } else {
            LOG_ERROR("HeapAllocator::Free called with unknown pointer");
        }
    }
#else
    m_allocation_count--;
    // Note: Can't track exact size without header in release mode
#endif
    
    ALIGNED_FREE(ptr);
}

void HeapAllocator::Reset() {
#ifdef _DEBUG
    std::lock_guard lock(m_debug_mutex);
    
    if (!m_allocations.empty()) {
        LOG_WARN("HeapAllocator::Reset - freeing {} leaked allocations:", m_allocations.size());
        for (auto& [ptr, size] : m_allocations) {
            LOG_WARN("  Leak: {} bytes at {}", size, ptr);
            ALIGNED_FREE(ptr);
        }
        m_allocations.clear();
    }
#endif
    
    m_allocated_size = 0;
    m_allocation_count = 0;
}

// Frame Allocator
FrameAllocator::FrameAllocator(size_t capacity_per_frame)
    : m_buffers{LinearAllocator(capacity_per_frame), LinearAllocator(capacity_per_frame)}
{
}

void* FrameAllocator::Allocate(size_t size, size_t alignment) {
    return m_buffers[m_current].Allocate(size, alignment);
}

void FrameAllocator::SwapBuffers() {
    m_current = 1 - m_current;
    m_buffers[m_current].Reset();
}

// Global allocators
static HeapAllocator g_heap_allocator;
static std::unique_ptr<FrameAllocator> g_frame_allocator;

HeapAllocator& GetHeapAllocator() {
    return g_heap_allocator;
}

FrameAllocator& GetFrameAllocator() {
    if (!g_frame_allocator) {
        // 16 MB per frame buffer
        g_frame_allocator = std::make_unique<FrameAllocator>(16_MB);
    }
    return *g_frame_allocator;
}

} // namespace action
