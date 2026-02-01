#pragma once

#include "core/types.h"
#include "gameplay/ecs/ecs.h"
#include "imgui_renderer.h"
#include "panels/viewport_panel.h"
#include "panels/multi_viewport_panel.h"
#include "panels/scene_tree_panel.h"
#include "panels/inspector_panel.h"
#include "panels/console_panel.h"
#include "panels/gizmo_panel.h"
#include "commands/command.h"
#include "prefabs/prefab.h"
#include "assets/asset_hot_reloader.h"
#include "shader_graph/shader_graph_editor.h"
#include "project/project.h"
#include "project/scene_serializer.h"
#include <memory>
#include <functional>

namespace action {

// Forward declarations
class Engine;
class Renderer;
class Platform;
class ECS;
class AssetManager;
class WorldManager;

/*
 * Editor - Godot-style dockable editor interface
 * 
 * Features:
 * - Dockable panels (viewport, scene tree, inspector, console)
 * - Transform gizmos (future)
 * - Node selection and property editing
 * - Play/Stop controls
 */

struct EditorConfig {
    bool start_in_play_mode = false;
    bool dark_theme = true;
};

// Editor node linked to actual ECS entity
struct EditorNode {
    u32 id = 0;
    Entity entity = INVALID_ENTITY;  // Link to ECS entity
    std::string name;
    std::string type;
    std::vector<EditorNode> children;
    bool expanded = false;
    
    // Transform (basic properties)
    vec3 position{0, 0, 0};
    vec3 rotation{0, 0, 0};  // Euler angles in degrees
    vec3 scale{1, 1, 1};
    
    // Visual properties
    vec3 color{0.8f, 0.8f, 0.8f};  // Object color (RGB)
    
    // Visibility
    bool visible = true;
    
    // Mesh handle for mesh nodes
    MeshHandle mesh{0};
};

class Editor {
public:
    Editor() = default;
    ~Editor() = default;
    
    bool Initialize(Renderer& renderer, Platform& platform, ECS& ecs, 
                    AssetManager& assets, WorldManager& world, const EditorConfig& config = {});
    void Shutdown();
    
    // Call each frame
    void BeginFrame();
    void Update(float dt);
    void Render(VkCommandBuffer cmd);
    void EndFrame();
    
    // Mode
    bool IsPlayMode() const { return m_play_mode; }
    void SetPlayMode(bool play) { m_play_mode = play; }
    void TogglePlayMode() { m_play_mode = !m_play_mode; }
    
    // Scene tree access
    EditorNode& GetSceneRoot() { return m_scene_root; }
    void SetSelectedNode(u32 node_id);
    EditorNode* GetSelectedNode();
    
    // Multi-selection for prefabs
    std::vector<u32>& GetSelectedNodeIds() { return m_selected_node_ids; }
    void ClearSelection() { m_selected_node_ids.clear(); m_selected_node_id = 0; }
    bool IsNodeSelected(u32 node_id) const;
    
    // Add nodes - creates actual ECS entities
    EditorNode* AddNode(const std::string& type, EditorNode* parent = nullptr);
    void DeleteNode(u32 node_id);
    
    // Node access/modification for commands
    EditorNode* FindNode(u64 node_id);
    EditorNode* FindNode(u64 node_id, EditorNode& root);
    void SetNodeTransform(u64 node_id, const vec3& position, const vec3& rotation, const vec3& scale);
    void SetNodePosition(u64 node_id, const vec3& position);
    void SetNodeRotation(u64 node_id, const vec3& rotation);
    void SetNodeScale(u64 node_id, const vec3& scale);
    void SetNodeColor(u64 node_id, const vec3& color);
    void SetNodeName(u64 node_id, const std::string& name);
    void SetNodeParent(u64 node_id, u64 new_parent_id);
    
    // Undo/Redo
    void ExecuteCommand(std::unique_ptr<Command> command);
    bool Undo();
    bool Redo();
    bool CanUndo() const { return m_command_history.CanUndo(); }
    bool CanRedo() const { return m_command_history.CanRedo(); }
    std::string GetUndoDescription() const { return m_command_history.GetUndoDescription(); }
    std::string GetRedoDescription() const { return m_command_history.GetRedoDescription(); }
    CommandHistory& GetCommandHistory() { return m_command_history; }
    
    // Prefabs
    PrefabManager& GetPrefabManager() { return m_prefab_manager; }
    
    // Asset Hot Reload
    AssetHotReloader& GetHotReloader() { return m_hot_reloader; }
    
    // Project management
    bool NewProject();
    bool OpenProject();
    bool OpenProject(const std::string& path);
    bool SaveScene();
    bool SaveSceneAs();
    bool LoadScene(const std::string& path);
    bool HasActiveProject() const { return m_active_project != nullptr; }
    Project* GetActiveProject() { return m_active_project.get(); }
    const std::string& GetCurrentScenePath() const { return m_current_scene_path; }
    void SetSceneModified(bool modified) { m_scene_modified = modified; }
    bool IsSceneModified() const { return m_scene_modified; }
    
    // Log message (for console panel)
    void Log(const std::string& message, int level = 0);
    
    // Check if ImGui wants keyboard/mouse
    bool WantsKeyboard() const;
    bool WantsMouse() const;
    
    // Check if gizmo is being manipulated
    bool IsGizmoManipulating() const;
    
private:
    void SetupDockspace();
    void SetupStyle();
    void DrawMenuBar();
    void DrawToolbar();
    void DrawAddNodePopup();
    void DrawSavePrefabPopup();
    void DrawNewProjectPopup();
    void DrawUnsavedChangesPopup();
    
    // Helper to check for unsaved changes before action
    void PromptSaveBeforeAction(std::function<void()> on_proceed);
    void ClearCurrentScene();
    
    // Create mesh handles for primitives (cached)
    void CreatePrimitiveMeshes();
    
    // Sync EditorNode transforms to ECS/WorldManager
    void SyncTransforms();
    
    EditorConfig m_config;
    
    // ImGui Vulkan rendering
    std::unique_ptr<ImGuiRenderer> m_imgui_renderer;
    
    // Panels
    std::unique_ptr<ViewportPanel> m_viewport_panel;
    std::unique_ptr<MultiViewportPanel> m_multi_viewport_panel;
    std::unique_ptr<SceneTreePanel> m_scene_tree_panel;
    std::unique_ptr<InspectorPanel> m_inspector_panel;
    std::unique_ptr<ConsolePanel> m_console_panel;
    std::unique_ptr<GizmoPanel> m_gizmo_panel;
    std::unique_ptr<ShaderGraphEditor> m_shader_graph_editor;
    
    // Prefab system
    PrefabManager m_prefab_manager;
    
    // Asset hot reloading
    AssetHotReloader m_hot_reloader;
    
    // State
    bool m_play_mode = false;
    bool m_show_demo_window = false;
    bool m_show_add_node_popup = false;
    bool m_show_save_prefab_popup = false;
    bool m_show_new_project_popup = false;
    bool m_show_unsaved_changes_popup = false;
    char m_prefab_name_buffer[128] = "";
    char m_new_project_name[128] = "MyProject";
    char m_new_project_path[512] = "";
    
    // Pending action after save confirmation
    std::function<void()> m_pending_action;
    
    // Project management
    std::unique_ptr<Project> m_active_project;
    std::string m_current_scene_path;
    bool m_scene_modified = false;
    
    // Scene data
    EditorNode m_scene_root;
    u32 m_selected_node_id = 0;               // Primary selection (for inspector)
    std::vector<u32> m_selected_node_ids;     // Multi-selection (for prefabs)
    u32 m_next_node_id = 1;
    
    // Cached primitive meshes
    MeshHandle m_cube_mesh{0};
    MeshHandle m_sphere_mesh{0};
    MeshHandle m_plane_mesh{0};
    
    // Pending async import
    std::string m_pending_import_path;
    
    // References to engine systems
    Renderer* m_renderer = nullptr;
    Platform* m_platform = nullptr;
    ECS* m_ecs = nullptr;
    AssetManager* m_assets = nullptr;
    WorldManager* m_world = nullptr;
    
    // Undo/Redo history
    CommandHistory m_command_history;
};

} // namespace action
