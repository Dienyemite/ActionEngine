#include "asset_hot_reloader.h"
#include "core/logging.h"
#include <algorithm>

namespace action {

AssetHotReloader::~AssetHotReloader() {
    Shutdown();
}

void AssetHotReloader::Initialize(AssetManager* assets, Editor* editor) {
    m_assets = assets;
    m_editor = editor;
    m_importer.Initialize(assets);
    
    // Default import settings for Blender exports
    m_default_import_settings.scale = 1.0f;
    m_default_import_settings.flip_uvs = true;
    m_default_import_settings.source_up_axis = ImportSettings::UpAxis::Z;  // Blender uses Z-up
    
    LOG_INFO("AssetHotReloader initialized");
}

void AssetHotReloader::Shutdown() {
    Stop();
    m_watch_directories.clear();
    m_watched_files.clear();
    m_imported_assets.clear();
    LOG_INFO("AssetHotReloader shutdown");
}

void AssetHotReloader::AddWatchDirectory(const std::string& directory, bool recursive) {
    // Check if already watching
    for (const auto& [dir, rec] : m_watch_directories) {
        if (dir == directory) {
            LOG_WARN("Already watching directory: {}", directory);
            return;
        }
    }
    
    // Check if directory exists
    if (!std::filesystem::exists(directory)) {
        LOG_WARN("Watch directory does not exist, creating: {}", directory);
        try {
            std::filesystem::create_directories(directory);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create watch directory: {} - {}", directory, e.what());
            return;
        }
    }
    
    m_watch_directories.push_back({directory, recursive});
    
    // Initial scan
    ScanDirectory(directory, recursive);
    
    LOG_INFO("Added watch directory: {} (recursive={})", directory, recursive);
}

void AssetHotReloader::RemoveWatchDirectory(const std::string& directory) {
    auto it = std::remove_if(m_watch_directories.begin(), m_watch_directories.end(),
                             [&directory](const auto& pair) { return pair.first == directory; });
    
    if (it != m_watch_directories.end()) {
        m_watch_directories.erase(it, m_watch_directories.end());
        LOG_INFO("Removed watch directory: {}", directory);
    }
}

void AssetHotReloader::ClearWatchDirectories() {
    m_watch_directories.clear();
    m_watched_files.clear();
    LOG_INFO("Cleared all watch directories");
}

void AssetHotReloader::Start() {
    if (m_running) return;
    
    m_running = true;
    m_watch_thread = std::thread(&AssetHotReloader::WatchThread, this);
    
    LOG_INFO("AssetHotReloader started watching {} directories", m_watch_directories.size());
}

void AssetHotReloader::Stop() {
    if (!m_running) return;
    
    m_running = false;
    if (m_watch_thread.joinable()) {
        m_watch_thread.join();
    }
    
    LOG_INFO("AssetHotReloader stopped");
}

void AssetHotReloader::Update() {
    // Process pending changes on main thread
    std::vector<std::pair<std::string, FileWatchEvent>> changes;
    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        changes = std::move(m_pending_changes);
        m_pending_changes.clear();
    }
    
    for (const auto& [path, event] : changes) {
        OnFileChanged(path, event);
    }
}

void AssetHotReloader::RefreshAll() {
    LOG_INFO("Refreshing all watched directories...");
    
    for (const auto& [directory, recursive] : m_watch_directories) {
        ScanDirectory(directory, recursive);
    }
}

void AssetHotReloader::WatchThread() {
    while (m_running) {
        // Sleep for watch interval
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(m_watch_interval * 1000)));
        
        if (!m_running) break;
        
        // Scan all directories
        for (const auto& [directory, recursive] : m_watch_directories) {
            if (!std::filesystem::exists(directory)) continue;
            
            try {
                if (recursive) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                        if (!m_running) break;
                        if (!entry.is_regular_file()) continue;
                        
                        std::string path = entry.path().string();
                        if (!ShouldWatch(path)) continue;
                        
                        auto write_time = entry.last_write_time();
                        auto file_size = entry.file_size();
                        
                        auto it = m_watched_files.find(path);
                        if (it == m_watched_files.end()) {
                            // New file
                            WatchedFile wf;
                            wf.path = path;
                            wf.last_write_time = write_time;
                            wf.file_size = file_size;
                            wf.exists = true;
                            m_watched_files[path] = wf;
                            
                            std::lock_guard<std::mutex> lock(m_pending_mutex);
                            m_pending_changes.push_back({path, FileWatchEvent::Created});
                        } else if (it->second.last_write_time != write_time) {
                            // Modified
                            it->second.last_write_time = write_time;
                            it->second.file_size = file_size;
                            
                            std::lock_guard<std::mutex> lock(m_pending_mutex);
                            m_pending_changes.push_back({path, FileWatchEvent::Modified});
                        }
                    }
                } else {
                    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                        if (!m_running) break;
                        if (!entry.is_regular_file()) continue;
                        
                        std::string path = entry.path().string();
                        if (!ShouldWatch(path)) continue;
                        
                        auto write_time = entry.last_write_time();
                        auto file_size = entry.file_size();
                        
                        auto it = m_watched_files.find(path);
                        if (it == m_watched_files.end()) {
                            WatchedFile wf;
                            wf.path = path;
                            wf.last_write_time = write_time;
                            wf.file_size = file_size;
                            wf.exists = true;
                            m_watched_files[path] = wf;
                            
                            std::lock_guard<std::mutex> lock(m_pending_mutex);
                            m_pending_changes.push_back({path, FileWatchEvent::Created});
                        } else if (it->second.last_write_time != write_time) {
                            it->second.last_write_time = write_time;
                            it->second.file_size = file_size;
                            
                            std::lock_guard<std::mutex> lock(m_pending_mutex);
                            m_pending_changes.push_back({path, FileWatchEvent::Modified});
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Error scanning directory {}: {}", directory, e.what());
            }
        }
        
        // Check for deleted files
        for (auto it = m_watched_files.begin(); it != m_watched_files.end();) {
            if (!std::filesystem::exists(it->first)) {
                std::lock_guard<std::mutex> lock(m_pending_mutex);
                m_pending_changes.push_back({it->first, FileWatchEvent::Deleted});
                it = m_watched_files.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void AssetHotReloader::ScanDirectory(const std::string& directory, bool recursive) {
    if (!std::filesystem::exists(directory)) return;
    
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                if (!ShouldWatch(path)) continue;
                
                WatchedFile wf;
                wf.path = path;
                wf.last_write_time = entry.last_write_time();
                wf.file_size = entry.file_size();
                wf.exists = true;
                m_watched_files[path] = wf;
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                if (!ShouldWatch(path)) continue;
                
                WatchedFile wf;
                wf.path = path;
                wf.last_write_time = entry.last_write_time();
                wf.file_size = entry.file_size();
                wf.exists = true;
                m_watched_files[path] = wf;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error scanning directory {}: {}", directory, e.what());
    }
}

void AssetHotReloader::OnFileChanged(const std::string& path, FileWatchEvent event) {
    switch (event) {
        case FileWatchEvent::Created:
            LOG_INFO("Asset created: {}", path);
            if (m_auto_import) {
                ImportAsset(path);
            }
            break;
            
        case FileWatchEvent::Modified:
            LOG_INFO("Asset modified: {}", path);
            if (m_auto_import) {
                ImportAsset(path);
            }
            break;
            
        case FileWatchEvent::Deleted:
            LOG_INFO("Asset deleted: {}", path);
            // Remove from imported assets
            m_imported_assets.erase(path);
            break;
            
        case FileWatchEvent::Renamed:
            LOG_INFO("Asset renamed: {}", path);
            break;
    }
}

bool AssetHotReloader::ImportAsset(const std::string& path) {
    if (!m_importer.IsFormatSupported(path)) {
        LOG_WARN("Unsupported format: {}", path);
        return false;
    }
    
    LOG_INFO("Importing asset: {}", path);
    
    ImportResult result = m_importer.Import(path, m_default_import_settings);
    
    if (!result.success) {
        LOG_ERROR("Failed to import {}: {}", path, result.error_message);
        if (m_reload_callback) {
            m_reload_callback(path, false);
        }
        return false;
    }
    
    // Create mesh handles from imported data
    std::vector<MeshHandle> meshes = m_importer.CreateMeshes(result.scene, *m_assets);
    
    if (meshes.empty()) {
        LOG_WARN("No meshes imported from: {}", path);
        return false;
    }
    
    // Track imported asset
    ImportedAsset asset;
    asset.source_path = path;
    asset.asset_name = result.scene.meshes[0].name;
    asset.mesh_handle = meshes[0];  // Primary mesh
    asset.import_time = std::filesystem::file_time_type::clock::now();
    asset.settings = m_default_import_settings;
    
    m_imported_assets[path] = asset;
    
    LOG_INFO("Successfully imported {} ({} meshes, {} vertices, {:.1f}ms)",
             path, meshes.size(), result.scene.total_vertices, result.import_time_ms);
    
    if (m_reload_callback) {
        m_reload_callback(path, true);
    }
    
    return true;
}

bool AssetHotReloader::ShouldWatch(const std::string& path) const {
    // Get extension
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Supported formats
    return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx";
}

} // namespace action
