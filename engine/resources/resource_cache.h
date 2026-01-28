#pragma once

#include "resource.h"
#include <unordered_map>
#include <mutex>
#include <typeindex>

namespace action {

/*
 * ResourceCache - Caches loaded resources for reuse (Godot-style)
 * 
 * Features:
 * - Path-based caching (same path = same resource)
 * - Automatic reference counting
 * - LRU eviction when memory limit reached
 * - Type-safe resource retrieval
 */

struct ResourceCacheConfig {
    size_t max_memory = 512_MB;          // Maximum cache memory
    size_t gc_threshold = 400_MB;        // Start GC when this is reached
    float gc_target_ratio = 0.7f;        // Target 70% after GC
    bool allow_duplicates = false;       // Allow same path multiple times
};

class ResourceCache {
public:
    ResourceCache() = default;
    ~ResourceCache() = default;
    
    bool Initialize(const ResourceCacheConfig& config = {});
    void Shutdown();
    
    // ===== Caching =====
    
    // Add a resource to the cache
    void Add(Ref<Resource> resource);
    
    // Get a cached resource by path (nullptr if not cached)
    Ref<Resource> Get(const std::string& path) const;
    
    // Get typed resource
    template<typename T>
    Ref<T> GetAs(const std::string& path) const {
        Ref<Resource> res = Get(path);
        return res ? std::dynamic_pointer_cast<T>(res) : nullptr;
    }
    
    // Check if resource is cached
    bool Has(const std::string& path) const;
    
    // Remove from cache
    void Remove(const std::string& path);
    void Remove(ResourceID id);
    
    // Clear all cached resources
    void Clear();
    
    // ===== Memory Management =====
    
    // Get total memory used by cached resources
    size_t GetMemoryUsage() const { return m_memory_usage; }
    size_t GetMaxMemory() const { return m_config.max_memory; }
    
    // Run garbage collection (remove unreferenced resources)
    void GarbageCollect();
    
    // Force evict resources to reach target memory
    void EvictToMemory(size_t target_bytes);
    
    // ===== Statistics =====
    size_t GetResourceCount() const { return m_resources_by_path.size(); }
    
    // Get all resources of a specific type
    template<typename T>
    std::vector<Ref<T>> GetResourcesOfType() const;
    
    // Iterate all cached resources
    template<typename Func>
    void ForEach(Func&& func) const;
    
private:
    struct CacheEntry {
        Ref<Resource> resource;
        float last_access_time = 0;
        size_t memory_size = 0;
    };
    
    void UpdateMemoryUsage();
    void EvictLRU();
    
    ResourceCacheConfig m_config;
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, CacheEntry> m_resources_by_path;
    std::unordered_map<ResourceID, std::string> m_path_by_id;
    
    size_t m_memory_usage = 0;
    float m_current_time = 0;
};

// Template implementations
template<typename T>
std::vector<Ref<T>> ResourceCache::GetResourcesOfType() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Ref<T>> result;
    
    for (const auto& [path, entry] : m_resources_by_path) {
        if (Ref<T> typed = std::dynamic_pointer_cast<T>(entry.resource)) {
            result.push_back(typed);
        }
    }
    
    return result;
}

template<typename Func>
void ResourceCache::ForEach(Func&& func) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [path, entry] : m_resources_by_path) {
        func(entry.resource);
    }
}

} // namespace action
