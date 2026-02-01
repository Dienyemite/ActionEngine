#include "project.h"
#include "serialization/json_format.h"
#include "core/logging.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace action {

// ===== ProjectSettings =====

SerialObject ProjectSettings::Serialize() const {
    SerialObject obj;
    obj.type_name = "ProjectSettings";
    
    obj.Set("window_width", static_cast<i64>(window_width));
    obj.Set("window_height", static_cast<i64>(window_height));
    obj.Set("fullscreen", fullscreen);
    obj.Set("vsync", vsync);
    
    obj.Set("shadow_resolution", static_cast<i64>(shadow_resolution));
    obj.Set("shadow_cascades", static_cast<i64>(shadow_cascades));
    obj.Set("draw_distance", static_cast<f64>(draw_distance));
    obj.Set("bloom_enabled", bloom_enabled);
    
    // Gravity as nested object
    SerialObject grav_obj;
    grav_obj.Set("x", static_cast<f64>(gravity.x));
    grav_obj.Set("y", static_cast<f64>(gravity.y));
    grav_obj.Set("z", static_cast<f64>(gravity.z));
    obj.SetObject("gravity", grav_obj);
    
    obj.Set("physics_substeps", static_cast<i64>(physics_substeps));
    
    return obj;
}

ProjectSettings ProjectSettings::Deserialize(const SerialObject& obj) {
    ProjectSettings settings;
    
    settings.window_width = static_cast<u32>(obj.Get<i64>("window_width", 1920));
    settings.window_height = static_cast<u32>(obj.Get<i64>("window_height", 1080));
    settings.fullscreen = obj.Get<bool>("fullscreen", false);
    settings.vsync = obj.Get<bool>("vsync", true);
    
    settings.shadow_resolution = static_cast<u32>(obj.Get<i64>("shadow_resolution", 1024));
    settings.shadow_cascades = static_cast<u32>(obj.Get<i64>("shadow_cascades", 2));
    settings.draw_distance = static_cast<float>(obj.Get<f64>("draw_distance", 400.0));
    settings.bloom_enabled = obj.Get<bool>("bloom_enabled", true);
    
    if (auto* grav = obj.GetObject("gravity")) {
        settings.gravity.x = static_cast<float>(grav->Get<f64>("x", 0.0));
        settings.gravity.y = static_cast<float>(grav->Get<f64>("y", -25.0));
        settings.gravity.z = static_cast<float>(grav->Get<f64>("z", 0.0));
    }
    
    settings.physics_substeps = static_cast<u32>(obj.Get<i64>("physics_substeps", 1));
    
    return settings;
}

// ===== Project Static Factory Methods =====

std::unique_ptr<Project> Project::Create(const std::string& project_name, const std::string& parent_directory) {
    auto project = std::make_unique<Project>();
    
    // Full project path is parent_directory/project_name
    std::string project_path = parent_directory + "/" + project_name;
    
    project->m_project_path = project_path;
    project->m_name = project_name;
    project->m_project_file_path = project_path + "/" + project_name + PROJECT_EXTENSION;
    
    // Create directory structure
    if (!project->CreateDirectoryStructure()) {
        LOG_ERROR("[Project] Failed to create directory structure at: {}", project_path);
        return nullptr;
    }
    
    // Create default main scene
    project->m_scenes.clear();
    project->m_scenes.push_back("main");
    project->m_active_scene = "main";
    
    // Reset settings to defaults
    project->m_settings = ProjectSettings{};
    
    // Save project file
    if (!project->Save()) {
        LOG_ERROR("[Project] Failed to save project file");
        return nullptr;
    }
    
    project->m_is_open = true;
    
    // Add to recent projects
    AddToRecentProjects(project->m_name, project->m_project_file_path);
    
    LOG_INFO("[Project] Created new project: {} at {}", project->m_name, project->m_project_path);
    return project;
}

std::unique_ptr<Project> Project::Open(const std::string& project_file_path) {
    // Check file exists
    if (!std::filesystem::exists(project_file_path)) {
        LOG_ERROR("[Project] Project file not found: {}", project_file_path);
        return nullptr;
    }
    
    // Load project file
    SerialObject obj = JsonFormat::LoadFromFile(project_file_path);
    if (obj.type_name.empty()) {
        LOG_ERROR("[Project] Failed to parse project file: {}", project_file_path);
        return nullptr;
    }
    
    auto project = std::make_unique<Project>();
    
    if (!project->DeserializeProject(obj)) {
        LOG_ERROR("[Project] Failed to deserialize project");
        return nullptr;
    }
    
    project->m_project_file_path = project_file_path;
    project->m_project_path = std::filesystem::path(project_file_path).parent_path().string();
    project->m_is_open = true;
    
    // Add to recent projects
    AddToRecentProjects(project->m_name, project->m_project_file_path);
    
    LOG_INFO("[Project] Opened project: {} from {}", project->m_name, project->m_project_path);
    return project;
}

// ===== Project Instance Methods =====

bool Project::Save() {
    if (m_project_file_path.empty()) {
        LOG_ERROR("[Project] No project file path set");
        return false;
    }
    
    SerialObject obj = SerializeProject();
    
    if (!JsonFormat::SaveToFile(m_project_file_path, obj, true)) {
        LOG_ERROR("[Project] Failed to write project file: {}", m_project_file_path);
        return false;
    }
    
    LOG_INFO("[Project] Saved project: {}", m_name);
    return true;
}

void Project::Close() {
    m_is_open = false;
    m_name.clear();
    m_project_path.clear();
    m_project_file_path.clear();
    m_scenes.clear();
    m_active_scene.clear();
    m_settings = ProjectSettings{};
    
    LOG_INFO("[Project] Closed project");
}

void Project::AddScene(const std::string& scene_name) {
    // Check if scene already exists
    for (const auto& s : m_scenes) {
        if (s == scene_name) return;
    }
    m_scenes.push_back(scene_name);
}

void Project::RemoveScene(const std::string& scene_name) {
    m_scenes.erase(
        std::remove(m_scenes.begin(), m_scenes.end(), scene_name),
        m_scenes.end()
    );
    
    // Clear active scene if it was removed
    if (m_active_scene == scene_name) {
        m_active_scene = m_scenes.empty() ? "" : m_scenes[0];
    }
}

std::string Project::GetScenePath(const std::string& scene_name) const {
    return GetScenesDirectory() + "/" + scene_name + SCENE_EXTENSION;
}

std::string Project::GetScenesDirectory() const {
    return m_project_path + "/scenes";
}

std::string Project::GetAssetsDirectory() const {
    return m_project_path + "/assets";
}

std::string Project::GetScriptsDirectory() const {
    return m_project_path + "/scripts";
}

bool Project::CreateDirectoryStructure() {
    try {
        std::filesystem::create_directories(m_project_path);
        std::filesystem::create_directories(GetScenesDirectory());
        std::filesystem::create_directories(GetAssetsDirectory());
        std::filesystem::create_directories(GetAssetsDirectory() + "/models");
        std::filesystem::create_directories(GetAssetsDirectory() + "/textures");
        std::filesystem::create_directories(GetScriptsDirectory());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[Project] Failed to create directories: {}", e.what());
        return false;
    }
}

SerialObject Project::SerializeProject() const {
    SerialObject obj;
    obj.type_name = "ActionEngineProject";
    
    obj.Set("version", static_cast<i64>(PROJECT_VERSION));
    obj.Set("name", m_name);
    obj.Set("active_scene", m_active_scene);
    
    // Scenes as array
    std::vector<SerialObject> scene_objs;
    for (const auto& scene : m_scenes) {
        SerialObject s;
        s.Set("name", scene);
        scene_objs.push_back(s);
    }
    obj.SetArray("scenes", scene_objs);
    
    // Settings
    obj.SetObject("settings", m_settings.Serialize());
    
    return obj;
}

bool Project::DeserializeProject(const SerialObject& obj) {
    if (obj.type_name != "ActionEngineProject") {
        LOG_ERROR("[Project] Invalid project file type: {}", obj.type_name);
        return false;
    }
    
    i64 version = obj.Get<i64>("version", 0);
    if (version > PROJECT_VERSION) {
        LOG_WARN("[Project] Project version {} is newer than supported {}", version, PROJECT_VERSION);
    }
    
    m_name = obj.Get<std::string>("name", "Untitled");
    m_active_scene = obj.Get<std::string>("active_scene", "");
    
    // Load scenes
    m_scenes.clear();
    if (auto* scenes = obj.GetArray("scenes")) {
        for (const auto& s : *scenes) {
            m_scenes.push_back(s.Get<std::string>("name", ""));
        }
    }
    
    // Load settings
    if (auto* settings = obj.GetObject("settings")) {
        m_settings = ProjectSettings::Deserialize(*settings);
    }
    
    return true;
}

// ===== Recent Projects =====

std::string Project::GetRecentProjectsPath() {
    // Store in user's app data
    const char* appdata = std::getenv("LOCALAPPDATA");
    if (appdata) {
        std::string path = std::string(appdata) + "/ActionEngine";
        std::filesystem::create_directories(path);
        return path + "/recent_projects.json";
    }
    return "recent_projects.json";
}

std::vector<RecentProject> Project::GetRecentProjects() {
    std::vector<RecentProject> recent;
    
    std::string path = GetRecentProjectsPath();
    if (!std::filesystem::exists(path)) {
        return recent;
    }
    
    SerialObject obj = JsonFormat::LoadFromFile(path);
    if (auto* projects = obj.GetArray("projects")) {
        for (const auto& p : *projects) {
            RecentProject rp;
            rp.name = p.Get<std::string>("name", "");
            rp.path = p.Get<std::string>("path", "");
            rp.last_opened = p.Get<std::string>("last_opened", "");
            
            // Only add if project file still exists
            if (std::filesystem::exists(rp.path)) {
                recent.push_back(rp);
            }
        }
    }
    
    return recent;
}

void Project::AddToRecentProjects(const std::string& name, const std::string& path) {
    auto recent = GetRecentProjects();
    
    // Remove if already exists
    recent.erase(
        std::remove_if(recent.begin(), recent.end(),
            [&path](const RecentProject& rp) { return rp.path == path; }),
        recent.end()
    );
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    // Add to front
    RecentProject rp;
    rp.name = name;
    rp.path = path;
    rp.last_opened = ss.str();
    recent.insert(recent.begin(), rp);
    
    // Keep only last 10
    if (recent.size() > 10) {
        recent.resize(10);
    }
    
    // Save
    SerialObject obj;
    obj.type_name = "RecentProjects";
    
    std::vector<SerialObject> arr;
    for (const auto& r : recent) {
        SerialObject p;
        p.Set("name", r.name);
        p.Set("path", r.path);
        p.Set("last_opened", r.last_opened);
        arr.push_back(p);
    }
    obj.SetArray("projects", arr);
    
    JsonFormat::SaveToFile(GetRecentProjectsPath(), obj, true);
}

void Project::ClearRecentProjects() {
    std::filesystem::remove(GetRecentProjectsPath());
}

} // namespace action
