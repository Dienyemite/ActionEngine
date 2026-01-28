#include "resource_cache.h"
#include "core/logging.h"
#include <algorithm>

namespace action {

bool ResourceCache::Initialize(const ResourceCacheConfig& config) {
    m_config = config;
    LOG_INFO("ResourceCache initialized (max: %zuMB)", config.max_memory / (1024 * 1024));
    return true;
}

void ResourceCache::Shutdown() {
    Clear();
    LOG_INFO("ResourceCache shutdown");
}

void ResourceCache::Add(Ref<Resource> resource) {
    if (!resource) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const std::string& path = resource->GetPath();
    
    // Check for duplicates
    if (!m_config.allow_duplicates && !path.empty() && m_resources_by_path.count(path)) {
        LOG_WARN("Resource already cached: %s", path.c_str());
        return;
    }
    
    CacheEntry entry;
    entry.resource = resource;
    entry.last_access_time = m_current_time;
    entry.memory_size = resource->GetMemoryUsage();
    
    if (!path.empty()) {
        m_resources_by_path[path] = entry;
    }
    m_path_by_id[resource->GetID()] = path;
    
    m_memory_usage += entry.memory_size;
    
    // Check if we need to GC
    if (m_memory_usage > m_config.gc_threshold) {
        GarbageCollect();
    }
}

Ref<Resource> ResourceCache::Get(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resources_by_path.find(path);
    if (it != m_resources_by_path.end()) {
        // Update access time (const_cast for LRU tracking)
        const_cast<CacheEntry&>(it->second).last_access_time = m_current_time;
        return it->second.resource;
    }
    
    return nullptr;
}

bool ResourceCache::Has(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resources_by_path.count(path) > 0;
}

void ResourceCache::Remove(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resources_by_path.find(path);
    if (it != m_resources_by_path.end()) {
        m_memory_usage -= it->second.memory_size;
        m_path_by_id.erase(it->second.resource->GetID());
        m_resources_by_path.erase(it);
    }
}

void ResourceCache::Remove(ResourceID id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto path_it = m_path_by_id.find(id);
    if (path_it != m_path_by_id.end()) {
        auto res_it = m_resources_by_path.find(path_it->second);
        if (res_it != m_resources_by_path.end()) {
            m_memory_usage -= res_it->second.memory_size;
            m_resources_by_path.erase(res_it);
        }
        m_path_by_id.erase(path_it);
    }
}

void ResourceCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resources_by_path.clear();
    m_path_by_id.clear();
    m_memory_usage = 0;
}

void ResourceCache::GarbageCollect() {
    // Remove resources with no external references
    std::vector<std::string> to_remove;
    
    for (const auto& [path, entry] : m_resources_by_path) {
        // shared_ptr use_count of 1 means only cache holds the reference
        if (entry.resource.use_count() == 1) {
            to_remove.push_back(path);
        }
    }
    
    for (const auto& path : to_remove) {
        auto it = m_resources_by_path.find(path);
        if (it != m_resources_by_path.end()) {
            m_memory_usage -= it->second.memory_size;
            m_path_by_id.erase(it->second.resource->GetID());
            m_resources_by_path.erase(it);
        }
    }
    
    if (!to_remove.empty()) {
        LOG_DEBUG("GC: removed %zu unreferenced resources", to_remove.size());
    }
    
    // If still over limit, evict LRU
    if (m_memory_usage > m_config.gc_threshold) {
        size_t target = static_cast<size_t>(m_config.max_memory * m_config.gc_target_ratio);
        EvictToMemory(target);
    }
}

void ResourceCache::EvictToMemory(size_t target_bytes) {
    if (m_memory_usage <= target_bytes) return;
    
    // Sort by last access time
    std::vector<std::pair<std::string, float>> entries;
    for (const auto& [path, entry] : m_resources_by_path) {
        // Only evict resources with single reference (cache only)
        if (entry.resource.use_count() == 1) {
            entries.emplace_back(path, entry.last_access_time);
        }
    }
    
    // Sort oldest first
    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Evict until under target
    for (const auto& [path, _] : entries) {
        if (m_memory_usage <= target_bytes) break;
        
        auto it = m_resources_by_path.find(path);
        if (it != m_resources_by_path.end()) {
            m_memory_usage -= it->second.memory_size;
            m_path_by_id.erase(it->second.resource->GetID());
            m_resources_by_path.erase(it);
        }
    }
}

void ResourceCache::EvictLRU() {
    // Find oldest entry with single reference
    float oldest_time = m_current_time;
    std::string oldest_path;
    
    for (const auto& [path, entry] : m_resources_by_path) {
        if (entry.resource.use_count() == 1 && entry.last_access_time < oldest_time) {
            oldest_time = entry.last_access_time;
            oldest_path = path;
        }
    }
    
    if (!oldest_path.empty()) {
        Remove(oldest_path);
    }
}

void ResourceCache::UpdateMemoryUsage() {
    m_memory_usage = 0;
    for (const auto& [path, entry] : m_resources_by_path) {
        m_memory_usage += entry.resource->GetMemoryUsage();
    }
}

} // namespace action
