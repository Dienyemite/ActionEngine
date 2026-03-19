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
#include "panels/asset_inspector_panel.h"
#include "panels/asset_browser_panel.h"
#include "panels/history_panel.h"
#include "panels/groups_panel.h"
#include "commands/command.h"
#include "prefabs/prefab.h"
#include "assets/asset_hot_reloader.h"
#include "shader_graph/shader_graph_editor.h"
#include "project/project.h"
#include "project/scene_serializer.h"
#include <imgui/imgui.h>
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
class AnimationLibrary;

/*
 * Editor - ImGui-based game editor
 * - Node selection and property editing
 * - Play/Stop controls
 */

struct EditorConfig {
    bool start_in_play_mode = false;
    bool dark_theme = true;
};

// Gizmo transform mode (matches Godot's toolbar buttons)
enum class TransformMode {
    Select  = 0,
    Move    = 1,
    Rotate  = 2,
    Scale   = 3,
};

// Gizmo coordinate space
enum class TransformSpace {
    World = 0,
    Local = 1,
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

    // ---- Light properties ----
    vec3  light_color{1.0f, 0.95f, 0.9f};
    float light_intensity = 1.0f;
    float light_range = 10.0f;
    float light_inner_angle = 30.0f;
    float light_outer_angle = 45.0f;
    bool  light_cast_shadows = true;

    // ---- Camera properties ----
    float cam_fov = 75.0f;
    float cam_near = 0.1f;
    float cam_far = 2000.0f;
    int   cam_projection = 0;  // 0=Perspective, 1=Orthographic

    // ---- Physics body properties ----
    float phys_mass = 1.0f;
    float phys_friction = 0.5f;
    float phys_bounce = 0.0f;
    bool  phys_lock_rotation = false;

    // ---- Particle properties ----
    int   particles_amount = 100;
    float particles_lifetime = 1.0f;
    float particles_speed_scale = 1.0f;

    // ---- Audio properties ----
    float audio_volume_db = 0.0f;
    float audio_pitch = 1.0f;
    bool  audio_autoplay = false;
    bool  audio_loop = false;
};

class Editor {
public:
    Editor() = default;
    ~Editor() = default;
    
    bool Initialize(Renderer& renderer, Platform& platform, ECS& ecs, 
                    AssetManager& assets, WorldManager& world, const EditorConfig& config = {});
    void Shutdown();

    // Inject animation library (called from Engine after it is created)
    void SetAnimationLibrary(AnimationLibrary* lib);

    // Inject script system (called from Engine after it is created)
    void SetScriptSystem(ScriptSystem* scripts);
    
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

    // Load a mesh from a file path and add it as a Mesh node in the scene.
    // This is the entry point used by the Asset Browser.
    void AddMeshFromFile(const std::string& path);

    // Generate a C++ script template in game/scripts/ and open it in VS Code.
    void CreateScriptFile(const std::string& script_name);

    void DeleteNode(u32 node_id);
    
    // Node access/modification for commands
    EditorNode* FindNode(u64 node_id);
    EditorNode* FindNode(u64 node_id, EditorNode& root);
    EditorNode* FindParentOf(u64 node_id);
    EditorNode* FindParentOf(u64 node_id, EditorNode& root);
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
    
    // Viewport picking from raw screen coordinates (called by Engine)
    void TryPickAtScreenPosition(float screen_x, float screen_y);
    
private:
    void SetupDockspace();
    void BuildDefaultLayout(ImGuiID dockspace_id);   // First-run Godot layout
    void SetupStyle();
    void DrawMenuBar();
    void DrawToolbar();
    void DrawAddNodePopup();
    void DrawSavePrefabPopup();
    void DrawNewProjectPopup();
    void DrawUnsavedChangesPopup();
    void DrawProjectSettingsDialog();
    void DrawExportDialog();
    
    // Helper to check for unsaved changes before action
    void PromptSaveBeforeAction(std::function<void()> on_proceed);
    void ClearCurrentScene();
    
    // Viewport picking
    void HandleViewportPick(const Ray& ray);
    EditorNode* FindNodeByEntity(Entity entity);
    EditorNode* FindNodeByEntity(Entity entity, EditorNode& root);
    
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
    std::unique_ptr<AssetInspectorPanel> m_asset_inspector_panel;
    std::unique_ptr<AssetBrowserPanel>   m_asset_browser_panel;
    std::unique_ptr<HistoryPanel>        m_history_panel;
    std::unique_ptr<GroupsPanel>         m_groups_panel;
    
    // Prefab system
    PrefabManager m_prefab_manager;
    
    // Asset hot reloading
    AssetHotReloader m_hot_reloader;
    
    // State
    bool m_play_mode = false;
    bool m_paused = false;
    bool m_show_demo_window = false;
    bool m_show_add_node_popup = false;
    bool m_show_save_prefab_popup = false;
    bool m_show_new_project_popup = false;
    bool m_show_unsaved_changes_popup = false;
    bool m_show_project_settings = false;
    bool m_show_export_dialog = false;
    bool m_layout_initialized = false;  // True once Godot default layout has been built

    // Transform toolbar
    TransformMode  m_transform_mode  = TransformMode::Move;
    TransformSpace m_transform_space = TransformSpace::World;
    bool           m_snap_enabled = false;
    float          m_snap_translate = 0.25f;
    float          m_snap_rotate    = 15.0f;
    float          m_snap_scale     = 0.25f;
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
    ScriptSystem* m_scripts = nullptr;
    
    // Undo/Redo history
    CommandHistory m_command_history;
};

} // namespace action
