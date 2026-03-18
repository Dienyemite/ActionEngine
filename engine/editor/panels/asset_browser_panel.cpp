#include "asset_browser_panel.h"
#include "core/logging.h"
#include <imgui/imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace action {

static const char* k_supported_exts[] = {
    ".glb", ".gltf", ".fbx", ".obj", ".dae", ".blend", ".3ds", ".stl", ".ply"
};

bool AssetBrowserPanel::IsSupportedMesh(const std::string& ext) const {
    for (const char* e : k_supported_exts) {
        if (ext == e) return true;
    }
    return false;
}

void AssetBrowserPanel::SetRootDirectory(const std::string& dir) {
    m_root_dir = dir;
    m_needs_refresh = true;
}

void AssetBrowserPanel::Refresh() {
    m_assets.clear();

    namespace fs = std::filesystem;

    if (!fs::exists(m_root_dir)) {
        LOG_WARN("[AssetBrowser] Directory not found: {}", m_root_dir);
        return;
    }

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(m_root_dir, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        // Convert to lower-case
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (!IsSupportedMesh(ext)) continue;

        AssetEntry ae;
        ae.display_name = entry.path().stem().string();
        ae.extension    = ext;

        // Store as a forward-slash relative path from cwd
        std::string full = entry.path().string();
        // Replace backslashes
        for (char& c : full) if (c == '\\') c = '/';
        ae.path = full;

        m_assets.push_back(std::move(ae));
    }

    // Sort by display name
    std::sort(m_assets.begin(), m_assets.end(),
              [](const AssetEntry& a, const AssetEntry& b) {
                  return a.display_name < b.display_name;
              });

    LOG_INFO("[AssetBrowser] Found {} mesh assets", m_assets.size());
    m_needs_refresh = false;
}

void AssetBrowserPanel::Draw() {
    if (!visible) return;

    if (m_needs_refresh) {
        Refresh();
    }

    if (!ImGui::Begin("Asset Browser", &visible)) {
        ImGui::End();
        return;
    }

    // Toolbar
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        Refresh();
    }

    ImGui::Separator();

    // Asset count
    ImGui::TextDisabled("%zu asset(s) in '%s'", m_assets.size(), m_root_dir.c_str());
    ImGui::Spacing();

    // Asset list
    if (ImGui::BeginChild("##assetlist", ImVec2(0, 0), false)) {
        for (const auto& asset : m_assets) {
            // Filter
            if (m_filter[0] != '\0') {
                std::string name_lower = asset.display_name;
                std::string filter_lower = m_filter;
                for (char& c : name_lower)   c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (char& c : filter_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (name_lower.find(filter_lower) == std::string::npos) continue;
            }

            // Row
            ImGui::PushID(asset.path.c_str());

            // Icon based on extension
            const char* icon = "[3D]";
            if (asset.extension == ".glb" || asset.extension == ".gltf") icon = "[glTF]";
            else if (asset.extension == ".fbx") icon = "[FBX]";
            else if (asset.extension == ".obj") icon = "[OBJ]";

            ImGui::TextDisabled("%s", icon);
            ImGui::SameLine();

            // Selectable name — double-click places asset
            bool selected = false;
            if (ImGui::Selectable(asset.display_name.c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0) && m_place_cb) {
                    m_place_cb(asset.path);
                }
            }

            // Tooltip shows full path
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Path: %s\nDouble-click to place in scene", asset.path.c_str());
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##ctx")) {
                if (ImGui::MenuItem("Place in Scene")) {
                    if (m_place_cb) m_place_cb(asset.path);
                }
                ImGui::TextDisabled("%s", asset.path.c_str());
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (m_assets.empty()) {
            ImGui::TextDisabled("No mesh files found.");
            ImGui::TextDisabled("Put .glb / .fbx / .obj files in:");
            ImGui::TextDisabled("  assets/meshes/");
            ImGui::TextDisabled("  assets/models/");
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace action
