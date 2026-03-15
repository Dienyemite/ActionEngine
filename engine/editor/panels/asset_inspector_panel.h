#pragma once

/*
 * AssetInspectorPanel — editor panel for viewing and editing .aeimport sidecars.
 *
 * Features:
 *   - Scans the project's assets directory for *.aeimport files.
 *   - Lists discovered imports in a left-side list box.
 *   - When an import is selected, shows its settings (editable inline).
 *   - "Reimport" button re-runs AssetImporter::Import() with the current settings
 *     and refreshes any ECS entities that reference the affected meshes.
 *   - "Save Changes" writes the modified sidecar back to disk as JSON.
 *
 * Division of responsibility:
 *   - This panel owns the UI + on-disk persistence of .aeimport files.
 *   - AssetImporter owns the actual import pass (Assimp + sidecar override).
 *   - AnimationLibrary owns the runtime skeleton/clip storage.
 */

#include "assets/aeimport.h"
#include <string>
#include <vector>
#include <functional>

namespace action {

class AssetManager;
class AnimationLibrary;

struct AssetInspectorEntry {
    std::string sidecar_path;       // Full path of the .aeimport file
    std::string asset_name;         // Display name (stem of source_file)
    AEImport    data;               // Loaded sidecar (may be invalid if load failed)
    bool        dirty = false;      // True when in-memory edits were not saved
};

class AssetInspectorPanel {
public:
    AssetInspectorPanel() = default;
    ~AssetInspectorPanel() = default;

    // Call once after the project assets directory is known.
    void SetAssetsDirectory(const std::string& dir);
    void SetAssetManager(AssetManager* assets) { m_assets = assets; }
    void SetAnimationLibrary(AnimationLibrary* anim_lib) { m_anim_lib = anim_lib; }

    // Optional: callback fired after a successful reimport.
    using ReimportCallback = std::function<void(const std::string& sidecar_path)>;
    void SetReimportCallback(ReimportCallback cb) { m_reimport_cb = cb; }

    // Refresh the list of .aeimport files from disk.
    void Refresh();

    // Draw the panel.  Call inside your ImGui frame.
    void Draw();

    bool visible = true;

private:
    // Draw the left-side asset list.
    void DrawAssetList();

    // Draw the right-side settings editor for the selected entry.
    void DrawSettings(AssetInspectorEntry& entry);

    // Draw the import settings block.
    bool DrawImportSettings(AEImportOverrides& s);

    // Draw the LOD config block.
    bool DrawLodConfig(AELodConfig& lod);

    // Draw the per-mesh overrides list.
    bool DrawMeshOverrides(std::vector<AEMeshOverride>& overrides);

    // Save the sidecar back to disk as JSON.
    bool SaveSidecar(const AssetInspectorEntry& entry);

    // Trigger a reimport of the selected asset.
    void TriggerReimport(AssetInspectorEntry& entry);

    std::string             m_assets_dir;
    std::vector<AssetInspectorEntry> m_entries;
    i32                     m_selected_index = -1;

    AssetManager*       m_assets   = nullptr;
    AnimationLibrary*   m_anim_lib = nullptr;
    ReimportCallback    m_reimport_cb;

    // Status message shown in the panel footer
    std::string m_status_message;
    float       m_status_timer = 0.0f;
};

} // namespace action
