#include "resource_loader.h"
#include "core/logging.h"
#include <fstream>
#include <filesystem>

namespace action {

bool ResourceLoader::Initialize(ResourceCache& cache, const ResourceLoaderConfig& config) {
    m_cache = &cache;
    m_config = config;
    
    LOG_INFO("ResourceLoader initialized");
    return true;
}

void ResourceLoader::Shutdown() {
    m_loaders.clear();
    m_type_map.clear();
    m_cache = nullptr;
    LOG_INFO("ResourceLoader shutdown");
}

void ResourceLoader::RegisterLoader(const std::string& extension, LoaderFunc loader) {
    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    
    // Convert to lowercase for case-insensitive matching
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    
    m_loaders[ext] = loader;
    LOG_DEBUG("Registered loader for extension: %s", ext.c_str());
}

void ResourceLoader::RegisterLoader(const std::vector<std::string>& extensions, LoaderFunc loader) {
    for (const auto& ext : extensions) {
        RegisterLoader(ext, loader);
    }
}

Ref<Resource> ResourceLoader::Load(const std::string& path) {
    // Fast path: check cache without taking the load mutex.
    // ResourceCache::Get() is thread-safe (has its own internal mutex).
    if (m_config.use_cache && m_cache) {
        if (Ref<Resource> cached = m_cache->Get(path)) {
            return cached;
        }
    }

    // -----------------------------------------------------------------------
    // TOCTOU-safe serialization for concurrent loads of the same path.
    //
    // Without this block, two threads can both miss the cache check above,
    // both call LoadInternal(), and end up with two distinct Resource objects
    // for the same file -- only one of which gets cached.
    // -----------------------------------------------------------------------
    {
        std::unique_lock<std::mutex> lock(m_load_mutex);

        // Double-check cache while holding the load mutex so we don't race with
        // a thread that just finished loading and added to the cache.
        if (m_config.use_cache && m_cache) {
            if (Ref<Resource> cached = m_cache->Get(path)) {
                return cached;
            }
        }

        // If another thread is already loading this path, wait for it to finish
        // (cv.wait releases the lock while waiting, allowing other paths to proceed).
        m_load_cv.wait(lock, [&] { return m_loading_paths.count(path) == 0; });

        // Re-check cache: the thread we were waiting for may have just added it.
        if (m_config.use_cache && m_cache) {
            if (Ref<Resource> cached = m_cache->Get(path)) {
                return cached;
            }
        }

        // We are the first thread to load this path -- mark it as in-flight.
        m_loading_paths.insert(path);
    }  // Release lock before the (potentially slow) disk read

    // --- Load from disk (without holding the mutex) ---
    Ref<Resource> resource = LoadInternal(path);

    // --- Add to cache and unmark the in-flight path ---
    {
        std::lock_guard<std::mutex> lock(m_load_mutex);

        if (resource && m_config.use_cache && m_cache) {
            m_cache->Add(resource);
        }

        m_loading_paths.erase(path);
    }
    // Wake up any threads waiting on this path.
    m_load_cv.notify_all();

    return resource;
}

std::future<Ref<Resource>> ResourceLoader::LoadAsync(const std::string& path) {
    return std::async(std::launch::async, [this, path]() {
        return Load(path);
    });
}

void ResourceLoader::Preload(const std::string& path) {
    Load(path);  // Just load into cache
}

void ResourceLoader::Preload(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        Preload(path);
    }
}

bool ResourceLoader::Exists(const std::string& path) const {
    return std::filesystem::exists(path);
}

std::string ResourceLoader::GetExtension(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    
    std::string ext = path.substr(dot);
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    return ext;
}

std::string ResourceLoader::GetResourceType(const std::string& extension) const {
    auto it = m_type_map.find(extension);
    return it != m_type_map.end() ? it->second : "Resource";
}

Ref<Resource> ResourceLoader::LoadInternal(const std::string& path) {
    if (!Exists(path)) {
        LOG_ERROR("Resource not found: %s", path.c_str());
        return nullptr;
    }
    
    std::string ext = GetExtension(path);
    
    auto it = m_loaders.find(ext);
    if (it == m_loaders.end()) {
        LOG_ERROR("No loader registered for extension: %s", ext.c_str());
        return nullptr;
    }
    
    LOG_DEBUG("Loading resource: %s", path.c_str());
    
    Ref<Resource> resource = it->second(path);
    if (resource) {
        resource->SetPath(path);
        resource->SetState(ResourceState::Loaded);
    }
    
    return resource;
}

} // namespace action
