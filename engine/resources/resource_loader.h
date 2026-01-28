#pragma once

#include "resource.h"
#include "resource_cache.h"
#include <unordered_map>
#include <functional>
#include <future>

namespace action {

/*
 * ResourceLoader - Loads resources from disk (Godot-style)
 * 
 * Features:
 * - Automatic type detection by extension
 * - Custom loader registration
 * - Async loading support
 * - Integrated caching
 */

// Loader function type
using LoaderFunc = std::function<Ref<Resource>(const std::string& path)>;

struct ResourceLoaderConfig {
    bool use_cache = true;
    bool async_default = false;
    size_t async_thread_count = 2;
};

class ResourceLoader {
public:
    ResourceLoader() = default;
    ~ResourceLoader() = default;
    
    bool Initialize(ResourceCache& cache, const ResourceLoaderConfig& config = {});
    void Shutdown();
    
    // ===== Loader Registration =====
    
    // Register a loader for specific extensions
    void RegisterLoader(const std::string& extension, LoaderFunc loader);
    void RegisterLoader(const std::vector<std::string>& extensions, LoaderFunc loader);
    
    // ===== Loading =====
    
    // Load resource (uses cache if available)
    Ref<Resource> Load(const std::string& path);
    
    // Load with type hint
    template<typename T>
    Ref<T> Load(const std::string& path);
    
    // Async load
    std::future<Ref<Resource>> LoadAsync(const std::string& path);
    
    template<typename T>
    std::future<Ref<T>> LoadAsync(const std::string& path);
    
    // Preload (load into cache without returning)
    void Preload(const std::string& path);
    void Preload(const std::vector<std::string>& paths);
    
    // ===== State =====
    
    // Check if resource exists
    bool Exists(const std::string& path) const;
    
    // Get file extension
    static std::string GetExtension(const std::string& path);
    
    // Get recognized resource type for extension
    std::string GetResourceType(const std::string& extension) const;
    
    // ===== Cache Access =====
    ResourceCache* GetCache() { return m_cache; }
    
private:
    Ref<Resource> LoadInternal(const std::string& path);
    
    ResourceLoaderConfig m_config;
    ResourceCache* m_cache = nullptr;
    
    std::unordered_map<std::string, LoaderFunc> m_loaders;
    std::unordered_map<std::string, std::string> m_type_map;  // extension -> type name
};

// Template implementations
template<typename T>
Ref<T> ResourceLoader::Load(const std::string& path) {
    Ref<Resource> res = Load(path);
    return res ? std::dynamic_pointer_cast<T>(res) : nullptr;
}

template<typename T>
std::future<Ref<T>> ResourceLoader::LoadAsync(const std::string& path) {
    return std::async(std::launch::async, [this, path]() {
        return Load<T>(path);
    });
}

} // namespace action
