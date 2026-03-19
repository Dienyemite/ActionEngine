#include "asset_browser_panel.h"
#include "core/logging.h"
#include <imgui/imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace action {
namespace fs = std::filesystem;

static std::string ToLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string NormalizePath(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}

bool AssetBrowserPanel::IsMeshFile(const std::string& ext) const {
    static const char* k_mesh_exts[] = {
        ".glb", ".gltf", ".fbx", ".obj", ".dae", ".blend", ".3ds", ".stl", ".ply"
    };
    for (const char* e : k_mesh_exts)
        if (ext == e) return true;
    return false;
}

const char* AssetBrowserPanel::GetFileIcon(const std::string& ext) const {
    if (ext == ".glb" || ext == ".gltf") return "[glTF]";
    if (ext == ".fbx")                   return "[FBX] ";
    if (ext == ".obj")                   return "[OBJ] ";
    if (ext == ".dae")                   return "[DAE] ";
    if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
                                         return "[IMG] ";
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return "[SND] ";
    if (ext == ".aescene")               return "[SCN] ";
    if (ext == ".h"  || ext == ".cpp")   return "[SRC] ";
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return "[SHD] ";
    if (ext == ".json" || ext == ".ini" || ext == ".toml")  return "[CFG] ";
    return "[FILE]";
}

void AssetBrowserPanel::SetRootDirectory(const std::string& dir) {
    m_root_dir = dir;
    m_needs_refresh = true;
}

void AssetBrowserPanel::BuildTree(DirNode& node, const std::string& dir_path, const std::string& rel_base) {
    std::error_code ec;
    if (!fs::exists(dir_path, ec)) return;

    std::vector<fs::directory_entry> entries;
    for (auto& e : fs::directory_iterator(dir_path, ec))
        entries.push_back(e);

    std::sort(entries.begin(), entries.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            bool a_dir = a.is_directory();
            bool b_dir = b.is_directory();
            if (a_dir != b_dir) return a_dir > b_dir;  // Dirs first
            return a.path().filename().string() < b.path().filename().string();
        });

    for (const auto& e : entries) {
        std::string name = e.path().filename().string();
        std::string full = NormalizePath(e.path().string());
        std::string rel  = rel_base.empty() ? name : (rel_base + "/" + name);

        if (e.is_directory(ec)) {
            DirNode child;
            child.name      = name;
            child.full_path = full;
            BuildTree(child, full, rel);
            node.subdirs.push_back(std::move(child));
        } else if (e.is_regular_file(ec)) {
            FileEntry fe;
            fe.name      = name;
            fe.full_path = full;
            fe.rel_path  = rel;
            fe.extension = ToLower(e.path().extension().string());
            fe.is_dir    = false;
            node.files.push_back(std::move(fe));
        }
    }
}

void AssetBrowserPanel::Refresh() {
    m_root_node = DirNode{};
    m_root_node.name      = m_root_dir;
    m_root_node.full_path = NormalizePath(fs::absolute(m_root_dir).string());

    BuildTree(m_root_node, m_root_node.full_path, "");

    // Default to showing the root directory
    m_current_dir_files = m_root_node.files;
    m_current_dir_path  = m_root_node.full_path;

    m_needs_refresh = false;
}

void AssetBrowserPanel::DrawDirTree(const DirNode& node) {
    bool is_current = (node.full_path == m_current_dir_path);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.subdirs.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (is_current)           flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_current_dir_path  = node.full_path;
        m_current_dir_files = node.files;
    }
    if (open) {
        for (const auto& sub : node.subdirs) {
            DrawDirTree(sub);
        }
        ImGui::TreePop();
    }
}

void AssetBrowserPanel::DrawFileList() {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##fs_filter", "Filter...", m_filter, sizeof(m_filter));
    ImGui::Separator();

    if (ImGui::BeginChild("##fsfiles", ImVec2(0, 0))) {
        if (m_current_dir_files.empty()) {
            ImGui::TextDisabled("(empty directory)");
        }
        for (const auto& fe : m_current_dir_files) {
            // Filter
            if (m_filter[0] != '\0') {
                std::string n_low = ToLower(fe.name);
                std::string f_low = ToLower(m_filter);
                if (n_low.find(f_low) == std::string::npos) continue;
            }

            bool selected = (m_selected_path == fe.full_path);

            // Icon
            const char* icon = GetFileIcon(fe.extension);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.7f, 0.9f, 1.0f));
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            ImGui::PushID(fe.full_path.c_str());
            if (ImGui::Selectable(fe.name.c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selected_path = fe.full_path;
                if (ImGui::IsMouseDoubleClicked(0) && m_place_cb && IsMeshFile(fe.extension)) {
                    m_place_cb(fe.full_path);
                }
            }

            // Tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s%s", fe.rel_path.c_str(),
                    IsMeshFile(fe.extension) ? "Double-click to place in scene\n" : "",
                    fe.extension.c_str());
            }

            // Context menu
            if (ImGui::BeginPopupContextItem("##fscm")) {
                if (IsMeshFile(fe.extension)) {
                    if (ImGui::MenuItem("Place in Scene")) {
                        if (m_place_cb) m_place_cb(fe.full_path);
                    }
                    ImGui::Separator();
                }
                ImGui::TextDisabled("%s", fe.rel_path.c_str());
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void AssetBrowserPanel::Draw() {
    if (!visible) return;
    if (m_needs_refresh) Refresh();

    if (!ImGui::Begin("FileSystem", &visible)) {
        ImGui::End();
        return;
    }

    // Toolbar row
    if (ImGui::Button("Refresh")) Refresh();
    ImGui::SameLine();
    // Show current relative path
    std::string display_path = m_current_dir_path;
    // Strip absolute root prefix for display
    std::string abs_root = NormalizePath(fs::absolute(m_root_dir).string());
    if (display_path.rfind(abs_root, 0) == 0) {
        display_path = "res:/" + display_path.substr(abs_root.size());
    }
    ImGui::TextDisabled("%s", display_path.c_str());

    ImGui::Separator();

    // Split: left = dir tree, right = file list
    float avail = ImGui::GetContentRegionAvail().x;
    float left_w  = avail * 0.35f;
    float right_w = avail - left_w - ImGui::GetStyle().ItemSpacing.x;

    if (ImGui::BeginChild("##FSDirTree", ImVec2(left_w, 0), true)) {
        DrawDirTree(m_root_node);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("##FSFileList", ImVec2(right_w, 0), false)) {
        DrawFileList();
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace action

