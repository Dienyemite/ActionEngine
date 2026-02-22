#pragma once

#include "core/types.h"
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace action {

// Forward declarations
class ResourceLoader;

/*
 * Resource - Base class for all loadable assets (Godot-style)
 * 
 * Features:
 * - Reference counting for memory management
 * - Unique path for caching/deduplication
 * - Type identification
 * - Load/unload lifecycle
 */

// Resource unique ID
using ResourceID = u64;
constexpr ResourceID INVALID_RESOURCE_ID = 0;

// Resource state
enum class ResourceState : u8 {
    Unloaded,    // Not loaded, data not in memory
    Loading,     // Currently being loaded (async)
    Loaded,      // Fully loaded and ready
    Failed       // Load failed
};

class Resource : public std::enable_shared_from_this<Resource> {
public:
    Resource();
    explicit Resource(const std::string& path);
    virtual ~Resource();
    
    // Prevent copying
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    
    // ===== Identification =====
    ResourceID GetID() const { return m_id; }
    const std::string& GetPath() const { return m_path; }
    void SetPath(const std::string& path) { m_path = path; }
    
    // Type name for serialization
    virtual std::string GetTypeName() const { return "Resource"; }
    static std::string GetStaticTypeName() { return "Resource"; }
    
    // ===== State =====
    ResourceState GetState() const  { return m_state.load(std::memory_order_acquire); }
    bool IsLoaded()  const { return GetState() == ResourceState::Loaded; }
    bool IsLoading() const { return GetState() == ResourceState::Loading; }
    bool HasFailed() const { return GetState() == ResourceState::Failed; }
    
    // ===== Reference Counting =====
    // Lifetime is managed by shared_ptr (Ref<T>). Use shared_from_this() to
    // obtain additional references. GetRefCount() returns the shared_ptr use_count.
    // AddRef/Release are deprecated no-ops kept for API compatibility.
    void AddRef() {}
    void Release() {}
    long GetRefCount() const { return GetUseCount(); }
    long GetUseCount() const;
    
    // ===== Memory =====
    virtual size_t GetMemoryUsage() const { return sizeof(*this); }
    
    // ===== Lifecycle =====
    virtual bool Load() { return true; }
    virtual void Unload() {}
    virtual void Reload();
    
    // ===== Dirty State (for editor) =====
    bool IsDirty() const { return m_dirty; }
    void SetDirty(bool dirty = true) { m_dirty = dirty; }
    
    // ===== Import Settings (for reimporting) =====
    const std::unordered_map<std::string, std::string>& GetImportSettings() const { return m_import_settings; }
    void SetImportSetting(const std::string& key, const std::string& value) { 
        m_import_settings[key] = value; 
        m_dirty = true;
    }
    
protected:
    friend class ResourceLoader;
    friend class ResourceCache;
    
    void SetState(ResourceState state) { m_state.store(state, std::memory_order_release); }
    
private:
    static ResourceID s_next_id;
    
    ResourceID m_id = INVALID_RESOURCE_ID;
    std::string m_path;
    std::atomic<ResourceState> m_state{ResourceState::Unloaded};  // #42: thread-safe state
    bool m_dirty = false;
    
    std::unordered_map<std::string, std::string> m_import_settings;
};

// Shared pointer type for resources
template<typename T>
using Ref = std::shared_ptr<T>;

// Helper to create resources
template<typename T, typename... Args>
Ref<T> MakeResource(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace action
