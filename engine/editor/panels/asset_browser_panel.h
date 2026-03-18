#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <functional>

namespace action {

/*
 * AssetBrowserPanel — file-system browser for the assets/ directory.
 *
 * Scans assets/meshes, assets/models, and assets/ root for supported mesh
 * formats (.glb, .gltf, .fbx, .obj, .dae, .blend, .3ds, .stl, .ply).
 *
 * Double-clicking a file fires m_place_cb with the relative asset path, which
 * the Editor handles by calling LoadMeshSync + AddNode.
 */
class AssetBrowserPanel {
public:
    AssetBrowserPanel() = default;
    ~AssetBrowserPanel() = default;

    // Set root directory to scan (default: "assets").
    void SetRootDirectory(const std::string& dir);

    // Callback fired when the user wants to place an asset in the scene.
    using PlaceCallback = std::function<void(const std::string& path)>;
    void SetPlaceCallback(PlaceCallback cb) { m_place_cb = cb; }

    // Rescan the directory tree (called automatically on first Draw).
    void Refresh();

    // Draw the panel.
    void Draw();

    bool visible = true;

private:
    struct AssetEntry {
        std::string display_name;   // Filename without extension
        std::string path;           // Relative path to pass to LoadMeshSync
        std::string extension;      // Lower-case extension (.glb, .obj, …)
    };

    bool IsSupportedMesh(const std::string& ext) const;

    std::string           m_root_dir  = "assets";
    std::vector<AssetEntry> m_assets;
    char                  m_filter[128] = {};
    bool                  m_needs_refresh = true;
    PlaceCallback         m_place_cb;
};

} // namespace action
