#pragma once

#include "core/types.h"
#include "serialization/serialization.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace action {

/*
 * Project System - Manage game projects with save/load support
 * 
 * File formats:
 * - .aeproj   - Project file (metadata, settings, scene list)
 * - .aescene  - Scene file (entities, components, hierarchy)
 * 
 * Directory structure:
 *   MyProject/
 *     MyProject.aeproj       <- Main project file
 *     scenes/
 *       main.aescene
 *       level1.aescene
 *     assets/
 *       models/
 *       textures/
 *     scripts/
 */

// Project settings
struct ProjectSettings {
    // Window
    u32 window_width = 1920;
    u32 window_height = 1080;
    bool fullscreen = false;
    bool vsync = true;
    
    // Rendering
    u32 shadow_resolution = 1024;
    u32 shadow_cascades = 2;
    float draw_distance = 400.0f;
    bool bloom_enabled = true;
    
    // Physics
    vec3 gravity{0, -25.0f, 0};
    u32 physics_substeps = 1;
    
    // Serialize
    SerialObject Serialize() const;
    static ProjectSettings Deserialize(const SerialObject& obj);
};

// Recent project entry
struct RecentProject {
    std::string name;
    std::string path;
    std::string last_opened;  // ISO date string
};

// Main project class
class Project {
public:
    static constexpr const char* PROJECT_EXTENSION = ".aeproj";
    static constexpr const char* SCENE_EXTENSION = ".aescene";
    static constexpr u32 PROJECT_VERSION = 1;
    
    Project() = default;
    ~Project() = default;
    
    // === Static Factory Methods ===
    
    // Create a new project (returns unique_ptr for easy transfer)
    static std::unique_ptr<Project> Create(const std::string& project_name, const std::string& parent_directory);
    
    // Open an existing project from .aeproj file
    static std::unique_ptr<Project> Open(const std::string& project_file_path);
    
    // === Project Lifecycle ===
    
    // Save project metadata
    bool Save();
    
    // Close current project
    void Close();
    
    // === Getters ===
    
    bool IsOpen() const { return m_is_open; }
    const std::string& GetName() const { return m_name; }
    const std::string& GetPath() const { return m_project_path; }
    const std::string& GetProjectFilePath() const { return m_project_file_path; }
    ProjectSettings& GetSettings() { return m_settings; }
    const ProjectSettings& GetSettings() const { return m_settings; }
    
    // === Scene Management ===
    
    // Get list of scenes in project
    const std::vector<std::string>& GetScenes() const { return m_scenes; }
    
    // Add a scene to the project
    void AddScene(const std::string& scene_name);
    
    // Remove scene from project
    void RemoveScene(const std::string& scene_name);
    
    // Get/set active scene
    const std::string& GetActiveScene() const { return m_active_scene; }
    void SetActiveScene(const std::string& scene_name) { m_active_scene = scene_name; }
    
    // Get full path to a scene file
    std::string GetScenePath(const std::string& scene_name) const;
    
    // === Directory Helpers ===
    
    std::string GetScenesDirectory() const;
    std::string GetScenesPath() const { return GetScenesDirectory(); }
    std::string GetAssetsDirectory() const;
    std::string GetScriptsDirectory() const;
    
    // === Recent Projects (Static) ===
    
    static std::vector<RecentProject> GetRecentProjects();
    static void AddToRecentProjects(const std::string& name, const std::string& path);
    static void ClearRecentProjects();
    static std::string GetRecentProjectsPath();
    
private:
    bool m_is_open = false;
    std::string m_name;
    std::string m_project_path;       // Directory containing project
    std::string m_project_file_path;  // Full path to .aeproj file
    
    ProjectSettings m_settings;
    std::vector<std::string> m_scenes;
    std::string m_active_scene;
    
    // Create project directory structure
    bool CreateDirectoryStructure();
    
    // Serialize/deserialize project
    SerialObject SerializeProject() const;
    bool DeserializeProject(const SerialObject& obj);
};

} // namespace action
