#pragma once

#include "../types.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <new>

namespace action {

/*
 * Memory allocation strategy for low-end hardware:
 * 
 * 1. Linear Allocator - Fast bump allocation for per-frame data
 * 2. Pool Allocator - Fixed-size blocks for components/entities
 * 3. Stack Allocator - LIFO for temporary allocations
 * 4. General Heap - Fallback with tracking
 * 
 * Goal: Minimize fragmentation and allocation overhead
 */

// Memory alignment helpers
constexpr size_t DEFAULT_ALIGNMENT = 16; // SSE alignment

inline size_t AlignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

inline void* AlignPointer(void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return reinterpret_cast<void*>(AlignUp(addr, alignment));
}

// Base allocator interface
class Allocator {
public:
    virtual ~Allocator() = default;
    
    virtual void* Allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT) = 0;
    virtual void Free(void* ptr) = 0;
    virtual void Reset() = 0;
    
    virtual size_t GetAllocatedSize() const = 0;
    virtual size_t GetCapacity() const = 0;
    
    template<typename T, typename... Args>
    T* New(Args&&... args) {
        void* mem = Allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    template<typename T>
    void Delete(T* ptr) {
        if (ptr) {
            ptr->~T();
            Free(ptr);
        }
    }
};

/*
 * Linear Allocator (Bump Allocator)
 * 
 * Usage: Per-frame temporary allocations
 * - Zero-cost allocation (just bump a pointer)
 * - Reset at end of frame
 * - No individual frees
 */
class LinearAllocator : public Allocator {
public:
    LinearAllocator(size_t capacity);
    ~LinearAllocator();
    
    void* Allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT) override;
    void Free(void* ptr) override; // No-op
    void Reset() override;
    
    size_t GetAllocatedSize() const override { return m_offset; }
    size_t GetCapacity() const override { return m_capacity; }
    
    // Get marker for partial reset
    size_t GetMarker() const { return m_offset; }
    void ResetToMarker(size_t marker) { m_offset = marker; }
    
private:
    u8* m_memory;
    size_t m_capacity;
    size_t m_offset;
};

/*
 * Pool Allocator
 * 
 * Usage: Fixed-size objects (components, entities)
 * - O(1) allocation and free
 * - No fragmentation within pool
 * - Perfect for ECS components
 */
template<typename T>
class PoolAllocator : public Allocator {
public:
    PoolAllocator(size_t count) 
        : m_capacity(count)
        , m_allocated(0) 
    {
        // Guard against count == 0: `count - 1` would underflow to SIZE_MAX,
        // causing a loop that writes past all memory.
        if (count == 0) {
            m_memory    = nullptr;
            m_free_list = nullptr;
            m_block_size = 0;
            return;
        }

        // Ensure alignment
        size_t block_size = AlignUp(sizeof(T), alignof(T));
        block_size = std::max(block_size, sizeof(FreeNode));
        m_block_size = block_size;
        
        m_memory = static_cast<u8*>(::operator new(block_size * count, 
                                     std::align_val_t{alignof(T)}));
        
        // Build free list
        m_free_list = reinterpret_cast<FreeNode*>(m_memory);
        FreeNode* current = m_free_list;
        for (size_t i = 0; i < count - 1; ++i) {
            current->next = reinterpret_cast<FreeNode*>(m_memory + (i + 1) * block_size);
            current = current->next;
        }
        current->next = nullptr;
    }
    
    ~PoolAllocator() {
        if (m_memory) {
            ::operator delete(m_memory, std::align_val_t{alignof(T)});
        }
    }
    
    void* Allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT) override {
        (void)size; (void)alignment; // Fixed size
        
        if (!m_free_list) {
            return nullptr; // Pool exhausted
        }
        
        FreeNode* node = m_free_list;
        m_free_list = node->next;
        m_allocated++;
        
        return static_cast<void*>(node);
    }
    
    void Free(void* ptr) override {
        if (!ptr) return;
        
        FreeNode* node = static_cast<FreeNode*>(ptr);
        node->next = m_free_list;
        m_free_list = node;
        m_allocated--;
    }
    
    void Reset() override {
        if (m_capacity == 0 || !m_memory) {
            m_allocated = 0;
            return;
        }
        // Rebuild free list (doesn't call destructors!)
        m_free_list = reinterpret_cast<FreeNode*>(m_memory);
        FreeNode* current = m_free_list;
        for (size_t i = 0; i < m_capacity - 1; ++i) {
            current->next = reinterpret_cast<FreeNode*>(m_memory + (i + 1) * m_block_size);
            current = current->next;
        }
        current->next = nullptr;
        m_allocated = 0;
    }
    
    size_t GetAllocatedSize() const override { return m_allocated * m_block_size; }
    size_t GetCapacity() const override { return m_capacity * m_block_size; }
    
    size_t GetAllocatedCount() const { return m_allocated; }
    size_t GetMaxCount() const { return m_capacity; }
    bool IsFull() const { return m_free_list == nullptr; }
    
private:
    struct FreeNode {
        FreeNode* next;
    };
    
    u8* m_memory;
    FreeNode* m_free_list;
    size_t m_block_size;
    size_t m_capacity;
    size_t m_allocated;
};

/*
 * Stack Allocator
 * 
 * Usage: Temporary allocations with LIFO pattern
 * - Fast allocation
 * - Must free in reverse order
 * - Good for recursive algorithms
 */
class StackAllocator : public Allocator {
public:
    StackAllocator(size_t capacity);
    ~StackAllocator();
    
    void* Allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT) override;
    void Free(void* ptr) override;
    void Reset() override;
    
    size_t GetAllocatedSize() const override { return m_offset; }
    size_t GetCapacity() const override { return m_capacity; }
    
private:
    struct AllocationHeader {
        size_t prev_offset;
        size_t adjustment;
    };
    
    u8* m_memory;
    size_t m_capacity;
    size_t m_offset;
};

/*
 * Tracked Heap Allocator
 * 
 * Usage: General purpose with memory tracking
 * - Wraps system allocator
 * - Tracks allocations for debugging
 * - Detects leaks
 */
class HeapAllocator : public Allocator {
public:
    HeapAllocator() = default;
    ~HeapAllocator();
    
    void* Allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT) override;
    void Free(void* ptr) override;
    void Reset() override; // Frees all and reports leaks
    
    size_t GetAllocatedSize() const override { return m_allocated_size.load(); }
    size_t GetCapacity() const override { return SIZE_MAX; }
    
    size_t GetAllocationCount() const { return m_allocation_count.load(); }
    
private:
    std::atomic<size_t> m_allocated_size{0};
    std::atomic<size_t> m_allocation_count{0};
    
    // Allocation map used for size tracking in both Debug and Release.
    // Required because ALIGNED_FREE has no way to recover the original size
    // without a stored header (which would complicate alignment guarantees).
    // HeapAllocator is not a hot-path allocator, so the mutex overhead is acceptable.
    std::mutex m_mutex;
    std::unordered_map<void*, size_t> m_allocations;
};

/*
 * Frame Allocator
 * 
 * Double-buffered linear allocator for frame data
 * - Current frame writes to one buffer
 * - Previous frame data still accessible
 * - Swap at frame boundary
 */
class FrameAllocator {
public:
    FrameAllocator(size_t capacity_per_frame);
    
    void* Allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT);
    void SwapBuffers();
    
    LinearAllocator& Current() { return m_buffers[m_current]; }
    LinearAllocator& Previous() { return m_buffers[1 - m_current]; }
    
private:
    LinearAllocator m_buffers[2];
    u32 m_current = 0;
};

// Global allocators
HeapAllocator& GetHeapAllocator();
FrameAllocator& GetFrameAllocator();

} // namespace action
