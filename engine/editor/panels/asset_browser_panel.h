#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <functional>

namespace action {

/*
 * AssetBrowserPanel — Godot-style FileSystem dock.
 *
 * Shows all project files in a tree view with icons. Left pane is a
 * directory tree; right pane lists files in the current directory.
 * Double-clicking a mesh places it in the scene.
 *
 * The window name is "FileSystem" so it docks into the layout slot.
 */
class AssetBrowserPanel {
public:
    AssetBrowserPanel() = default;
    ~AssetBrowserPanel() = default;

    void SetRootDirectory(const std::string& dir);

    using PlaceCallback = std::function<void(const std::string& path)>;
    void SetPlaceCallback(PlaceCallback cb) { m_place_cb = cb; }

    void Refresh();
    void Draw();

    bool visible = true;

private:
    struct FileEntry {
        std::string name;
        std::string full_path;   // OS path
        std::string rel_path;    // Relative from root
        std::string extension;   // lowercase, e.g. ".glb"
        bool is_dir = false;
    };

    struct DirNode {
        std::string name;
        std::string full_path;
        std::vector<DirNode> subdirs;
        std::vector<FileEntry> files;
    };

    // Recursive tree builder
    void BuildTree(DirNode& node, const std::string& dir_path, const std::string& rel_base);

    // Draw the directory tree pane
    void DrawDirTree(const DirNode& node);

    // Draw the file list for the current directory
    void DrawFileList();

    const char* GetFileIcon(const std::string& ext) const;
    bool IsMeshFile(const std::string& ext) const;

    DirNode m_root_node;
    std::string m_root_dir = "assets";
    std::string m_current_dir_path;     // Currently browsed directory (full path)
    std::vector<FileEntry>* m_current_files = nullptr;  // Pointer into tree

    // For the flat "current dir" display
    std::vector<FileEntry> m_current_dir_files;

    char m_filter[128] = {};
    bool m_needs_refresh = true;

    PlaceCallback m_place_cb;
    std::string m_selected_path;
};

} // namespace action
