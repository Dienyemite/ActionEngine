#pragma once

#include "core/types.h"
#include "asset_importer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <future>

namespace action {

// Forward declarations
class AssetManager;
class Editor;
class JobSystem;

/*
 * PendingAsyncImport - Tracks an in-progress async import
 */
struct PendingAsyncImport {
    std::string filepath;
    std::future<ImportResult> future;
    std::chrono::steady_clock::time_point start_time;
};

/*
 * FileWatchEvent - Type of file change detected
 */
enum class FileWatchEvent {
    Created,
    Modified,
    Deleted,
    Renamed
};

/*
 * WatchedFile - Information about a watched file
 */
struct WatchedFile {
    std::string path;
    std::filesystem::file_time_type last_write_time;
    size_t file_size = 0;
    bool exists = false;
};

/*
 * ImportedAsset - Track of an imported asset
 */
struct ImportedAsset {
    std::string source_path;      // Original file path (e.g., Blender export)
    std::string asset_name;       // Name in engine
    MeshHandle mesh_handle;       // Handle to mesh (if applicable)
    std::filesystem::file_time_type import_time;
    ImportSettings settings;
};

/*
 * AssetHotReloader - Watches directories for asset changes and reloads
 * 
 * This enables seamless Blender workflow:
 * 1. User exports model from Blender to watched directory
 * 2. Hot reloader detects the new/modified file
 * 3. Asset is automatically imported/reimported
 * 4. Scene instances are updated with new geometry
 * 
 * Usage:
 *   hotreloader.AddWatchDirectory("assets/models");
 *   hotreloader.SetAutoImport(true);
 *   hotreloader.Start();
 *   
 *   // In frame loop:
 *   hotreloader.Update();  // Processes queued changes
 */
class AssetHotReloader {
public:
    AssetHotReloader() = default;
    ~AssetHotReloader();
    
    // Initialize with references to engine systems
    void Initialize(AssetManager* assets, Editor* editor, JobSystem* jobs = nullptr);
    void Shutdown();
    
    // Directory watching
    void AddWatchDirectory(const std::string& directory, bool recursive = true);
    void RemoveWatchDirectory(const std::string& directory);
    void ClearWatchDirectories();
    
    // Settings
    void SetAutoImport(bool enabled) { m_auto_import = enabled; }
    bool IsAutoImportEnabled() const { return m_auto_import; }
    
    void SetWatchInterval(float seconds) { m_watch_interval = seconds; }
    float GetWatchInterval() const { return m_watch_interval; }
    
    void SetDefaultImportSettings(const ImportSettings& settings) { 
        m_default_import_settings = settings; 
    }
    
    // Start/stop watching
    void Start();
    void Stop();
    bool IsRunning() const { return m_running; }
    bool IsWatching() const { return m_running; }  // Alias for IsRunning
    
    // Get watch directories
    const std::vector<std::pair<std::string, bool>>& GetWatchDirectories() const {
        return m_watch_directories;
    }
    
    // Call each frame to process pending changes (main thread)
    void Update();
    
    // Manual refresh
    void RefreshAll();
    
    // Get imported assets
    const std::unordered_map<std::string, ImportedAsset>& GetImportedAssets() const {
        return m_imported_assets;
    }
    
    // Callback when asset is imported/reloaded
    using AssetReloadCallback = std::function<void(const std::string& path, bool success)>;
    void SetReloadCallback(AssetReloadCallback callback) { m_reload_callback = callback; }
    
    // Manual import of a file (used by file dialog)
    bool ImportFile(const std::string& filepath);
    
    // Async import - returns immediately, check IsImportPending() and GetCompletedImport()
    void ImportFileAsync(const std::string& filepath);
    bool IsImportPending() const { return !m_pending_imports.empty(); }
    bool HasCompletedImport();
    
    // Get the importer for format checks
    AssetImporter& GetImporter() { return m_importer; }
    
    // Statistics
    u32 GetWatchedFileCount() const { return (u32)m_watched_files.size(); }
    u32 GetImportedAssetCount() const { return (u32)m_imported_assets.size(); }
    
private:
    // Background watching thread
    void WatchThread();
    
    // Check a single directory for changes
    void ScanDirectory(const std::string& directory, bool recursive);
    
    // Process file change
    void OnFileChanged(const std::string& path, FileWatchEvent event);
    
    // Import/reimport an asset
    bool ImportAsset(const std::string& path);
    
    // Check if a file should be watched
    bool ShouldWatch(const std::string& path) const;
    
    AssetManager* m_assets = nullptr;
    Editor* m_editor = nullptr;
    JobSystem* m_jobs = nullptr;
    AssetImporter m_importer;
    
    // Async import tracking
    std::vector<PendingAsyncImport> m_pending_imports;
    std::mutex m_pending_imports_mutex;
    
    // Watch configuration
    std::vector<std::pair<std::string, bool>> m_watch_directories;  // path, recursive
    float m_watch_interval = 0.5f;  // Seconds between scans
    bool m_auto_import = true;
    ImportSettings m_default_import_settings;
    
    // Watch state
    std::unordered_map<std::string, WatchedFile> m_watched_files;
    std::unordered_map<std::string, ImportedAsset> m_imported_assets;
    
    // Threading
    std::thread m_watch_thread;
    std::atomic<bool> m_running{false};
    std::mutex m_pending_mutex;
    std::vector<std::pair<std::string, FileWatchEvent>> m_pending_changes;
    
    // Callback
    AssetReloadCallback m_reload_callback;
};

} // namespace action
