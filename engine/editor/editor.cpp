#include "editor.h"
#include "core/logging.h"
#include "render/renderer.h"
#include "platform/platform.h"
#include "gameplay/ecs/ecs.h"
#include "assets/asset_manager.h"
#include "world/world_manager.h"
#include "commands/editor_commands.h"
#include <imgui/imgui.h>
#include <algorithm>

namespace action {

bool Editor::Initialize(Renderer& renderer, Platform& platform, ECS& ecs,
                        AssetManager& assets, WorldManager& world, const EditorConfig& config) {
    m_config = config;
    m_renderer = &renderer;
    m_platform = &platform;
    m_ecs = &ecs;
    m_assets = &assets;
    m_world = &world;
    
    LOG_INFO("Initializing Editor...");
    
    // Initialize ImGui renderer
    m_imgui_renderer = std::make_unique<ImGuiRenderer>();
    if (!m_imgui_renderer->Initialize(renderer.GetContext(), renderer, platform)) {
        LOG_ERROR("Failed to initialize ImGui renderer");
        return false;
    }
    
    // Apply theme
    SetupStyle();
    
    // Create panels
    m_viewport_panel = std::make_unique<ViewportPanel>();
    m_multi_viewport_panel = std::make_unique<MultiViewportPanel>();
    m_scene_tree_panel = std::make_unique<SceneTreePanel>();
    m_inspector_panel = std::make_unique<InspectorPanel>();
    m_console_panel = std::make_unique<ConsolePanel>();
    m_gizmo_panel = std::make_unique<GizmoPanel>();
    m_shader_graph_editor = std::make_unique<ShaderGraphEditor>();
    m_shader_graph_editor->Initialize();
    m_shader_graph_editor->visible = false;  // Start hidden
    
    // Set up Inspector delete callback
    m_inspector_panel->SetDeleteCallback([this](u32 node_id) {
        if (node_id != 0 && node_id != m_scene_root.id) {
            DeleteNode(node_id);
        }
    });
    
    // Create primitive meshes for adding nodes
    CreatePrimitiveMeshes();
    
    // Initialize prefab manager
    m_prefab_manager.Initialize(this, m_assets);
    
    // Initialize hot reloader for Blender assets
    m_hot_reloader.Initialize(m_assets, this);
    m_hot_reloader.AddWatchDirectory("assets/models", true);
    m_hot_reloader.SetReloadCallback([this](const std::string& path, bool success) {
        if (success) {
            Log("Asset reloaded: " + path, 0);
        } else {
            Log("Failed to reload asset: " + path, 2);
        }
    });
    m_hot_reloader.Start();
    
    // Setup initial scene tree with just a root node (empty scene)
    m_scene_root.id = m_next_node_id++;
    m_scene_root.name = "Scene";
    m_scene_root.type = "Node";
    m_scene_root.expanded = true;
    
    m_play_mode = config.start_in_play_mode;
    
    LOG_INFO("Editor initialized - empty scene");
    return true;
}

void Editor::CreatePrimitiveMeshes() {
    // Create cached primitive meshes for quick node creation
    m_cube_mesh = m_assets->CreateCubeMesh(1.0f);
    m_sphere_mesh = m_assets->CreateSphereMesh(0.5f, 16);
    m_plane_mesh = m_assets->CreatePlaneMesh(10.0f, 10.0f, 1, 1);
    
    LOG_INFO("Created primitive meshes for editor");
}

void Editor::Shutdown() {
    LOG_INFO("Editor shutting down...");
    
    // Shutdown hot reloader first
    m_hot_reloader.Shutdown();
    
    m_viewport_panel.reset();
    m_multi_viewport_panel.reset();
    m_scene_tree_panel.reset();
    m_inspector_panel.reset();
    m_console_panel.reset();
    m_gizmo_panel.reset();
    m_shader_graph_editor.reset();
    
    if (m_imgui_renderer) {
        m_imgui_renderer->Shutdown();
        m_imgui_renderer.reset();
    }
    
    LOG_INFO("Editor shutdown complete");
}

void Editor::BeginFrame() {
    m_imgui_renderer->BeginFrame();
}

void Editor::Update(float dt) {
    // Process hot reloaded assets
    m_hot_reloader.Update();
    
    // Check for completed async imports
    if (m_hot_reloader.HasCompletedImport() && !m_pending_import_path.empty()) {
        const auto& assets = m_hot_reloader.GetImportedAssets();
        auto it = assets.find(m_pending_import_path);
        if (it != assets.end() && it->second.mesh_handle.is_valid()) {
            MeshHandle imported_mesh = it->second.mesh_handle;
            std::string mesh_name = it->second.asset_name;
            if (mesh_name.empty()) {
                size_t last_slash = m_pending_import_path.find_last_of("\\/");
                size_t last_dot = m_pending_import_path.find_last_of('.');
                if (last_slash != std::string::npos && last_dot != std::string::npos) {
                    mesh_name = m_pending_import_path.substr(last_slash + 1, last_dot - last_slash - 1);
                } else {
                    mesh_name = "ImportedMesh";
                }
            }
            
            // Create editor node with the mesh
            EditorNode node;
            node.id = m_next_node_id++;
            node.name = mesh_name;
            node.type = "Mesh";
            node.position = {0, 0, 0};
            node.rotation = {0, 0, 0};
            node.scale = {1, 1, 1};
            node.mesh = imported_mesh;
            node.visible = true;
            
            // Create ECS entity
            Entity entity = m_ecs->CreateEntity();
            node.entity = entity;
            
            auto& transform = m_ecs->AddComponent<TransformComponent>(entity);
            transform.position = node.position;
            transform.rotation = quat::identity();
            transform.scale = node.scale;
            
            auto& render = m_ecs->AddComponent<RenderComponent>(entity);
            render.mesh = imported_mesh;
            render.visible = true;
            render.cast_shadow = true;
            
            auto& bounds = m_ecs->AddComponent<BoundsComponent>(entity);
            if (MeshData* mesh_data = m_assets->GetMesh(imported_mesh)) {
                bounds.local_bounds = mesh_data->bounds;
                bounds.world_bounds = mesh_data->bounds;
            }
            
            auto& tag = m_ecs->AddComponent<TagComponent>(entity);
            tag.name = node.name;
            
            m_scene_root.children.push_back(node);
            SetSelectedNode(node.id);
            
            Log("Created mesh node: " + mesh_name, 0);
        }
        m_pending_import_path.clear();
    }
    
    // Setup dockspace for the entire window
    SetupDockspace();
    
    // Draw menu bar
    DrawMenuBar();
    
    // Draw toolbar
    DrawToolbar();
    
    // Draw panels
    m_viewport_panel->Draw(*m_renderer, m_play_mode);
    m_multi_viewport_panel->Draw(*m_renderer, m_play_mode);
    m_scene_tree_panel->Draw(m_scene_root, m_selected_node_id, m_selected_node_ids);
    
    EditorNode* selected = GetSelectedNode();
    m_inspector_panel->Draw(selected);
    
    m_console_panel->Draw();
    
    // Draw gizmos for selected object (if any and not in play mode)
    if (selected && selected->entity != INVALID_ENTITY && !m_play_mode && m_viewport_panel->show_gizmos) {
        // Use full window coordinates since the 3D scene renders to the full swapchain
        // The gizmo will appear directly on the object in world space
        vec2 viewport_pos(0.0f, 0.0f);
        vec2 viewport_size((float)m_platform->GetWidth(), (float)m_platform->GetHeight());
        
        // Draw gizmo and update transform if manipulated
        if (m_gizmo_panel->Draw(*m_renderer, selected->position, selected->rotation, selected->scale,
                                 viewport_pos, viewport_size)) {
            // Gizmo was manipulated, transform will be synced below
        }
        
        // Check if gizmo drag just ended - create undo command
        if (m_gizmo_panel->DragJustEnded()) {
            vec3 old_pos = m_gizmo_panel->GetDragStartPosition();
            vec3 old_rot = m_gizmo_panel->GetDragStartRotation();
            vec3 old_scale = m_gizmo_panel->GetDragStartScale();
            
            // Create appropriate command based on what changed
            // Use AddWithoutExecute since the change is already applied
            if (old_pos.x != selected->position.x || old_pos.y != selected->position.y || old_pos.z != selected->position.z) {
                auto cmd = std::make_unique<TransformCommand>(
                    this, selected->id, TransformCommand::Type::Position,
                    old_pos, selected->position);
                m_command_history.AddWithoutExecute(std::move(cmd));
            }
            if (old_rot.x != selected->rotation.x || old_rot.y != selected->rotation.y || old_rot.z != selected->rotation.z) {
                auto cmd = std::make_unique<TransformCommand>(
                    this, selected->id, TransformCommand::Type::Rotation,
                    old_rot, selected->rotation);
                m_command_history.AddWithoutExecute(std::move(cmd));
            }
            if (old_scale.x != selected->scale.x || old_scale.y != selected->scale.y || old_scale.z != selected->scale.z) {
                auto cmd = std::make_unique<TransformCommand>(
                    this, selected->id, TransformCommand::Type::Scale,
                    old_scale, selected->scale);
                m_command_history.AddWithoutExecute(std::move(cmd));
            }
        }
    }
    
    // Sync transform changes from editor to ECS/WorldManager
    SyncTransforms();
    
    // Draw Add Node popup if requested
    DrawAddNodePopup();
    
    // Draw Save Prefab popup if requested
    DrawSavePrefabPopup();
    
    // Draw New Project popup if requested
    DrawNewProjectPopup();
    
    // Draw shader graph editor
    m_shader_graph_editor->Draw(*m_renderer);
    
    // Show demo window if enabled
    if (m_show_demo_window) {
        ImGui::ShowDemoWindow(&m_show_demo_window);
    }
}

void Editor::Render(VkCommandBuffer cmd) {
    m_imgui_renderer->Render(cmd);
}

void Editor::EndFrame() {
    m_imgui_renderer->EndFrame();
}

void Editor::SetupDockspace() {
    // Create a full-screen dockspace
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // Submit the DockSpace
    ImGuiID dockspace_id = ImGui::GetID("ActionEngineDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    
    ImGui::End();
}

void Editor::SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Godot-inspired dark theme
    ImVec4* colors = style.Colors;
    
    // Background colors
    colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.18f, 0.95f);
    
    // Title bar
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 0.75f);
    
    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    
    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Frame (input fields, checkboxes, etc.)
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    
    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.55f, 0.78f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.45f, 0.68f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.35f, 0.55f, 1.0f);
    
    // Headers
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    
    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.28f, 0.45f, 0.68f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.55f, 0.80f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.40f, 0.60f, 1.0f);
    
    // Separators
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.38f, 0.55f, 0.78f, 1.0f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.38f, 0.55f, 0.78f, 1.0f);
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.45f, 0.68f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.38f, 0.55f, 0.78f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.38f, 0.55f, 0.78f, 0.95f);
    
    // Docking
    colors[ImGuiCol_DockingPreview] = ImVec4(0.38f, 0.55f, 0.78f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    
    // Text
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    
    // Style adjustments
    style.WindowRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;
}

void Editor::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // Project management
            if (ImGui::MenuItem("New Project...", "Ctrl+Shift+N")) {
                // Initialize default project path
                #ifdef _WIN32
                    const char* userprofile = std::getenv("USERPROFILE");
                    if (userprofile) {
                        snprintf(m_new_project_path, sizeof(m_new_project_path), 
                                 "%s\\Documents\\ActionEngine Projects", userprofile);
                    }
                #endif
                m_show_new_project_popup = true;
            }
            if (ImGui::MenuItem("Open Project...", "Ctrl+Shift+O")) {
                OpenProject();
            }
            
            // Recent projects submenu
            if (ImGui::BeginMenu("Recent Projects")) {
                auto recent = Project::GetRecentProjects();
                if (recent.empty()) {
                    ImGui::TextDisabled("No recent projects");
                } else {
                    for (const auto& proj : recent) {
                        if (ImGui::MenuItem(proj.name.c_str())) {
                            OpenProject(proj.path);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s", proj.path.c_str());
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent")) {
                        Project::ClearRecentProjects();
                    }
                }
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            // Scene management
            bool has_project = HasActiveProject();
            if (ImGui::MenuItem("New Scene", "Ctrl+N", false, has_project)) {
                // Clear all children from scene root
                for (auto& child : m_scene_root.children) {
                    if (child.entity != INVALID_ENTITY) {
                        m_ecs->DestroyEntity(child.entity);
                    }
                }
                m_scene_root.children.clear();
                m_selected_node_id = 0;
                m_current_scene_path.clear();
                m_scene_modified = false;
                Log("Created new scene", 0);
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O", false, has_project)) {
                std::string filter = "Scene Files|*.aescene|All Files|*.*";
                std::string scenes_dir = m_active_project ? m_active_project->GetScenesPath() : "";
                std::string filepath = m_platform->OpenFileDialog("Open Scene", filter, scenes_dir);
                if (!filepath.empty()) {
                    LoadScene(filepath);
                }
            }
            
            // Show scene modified indicator
            std::string save_label = m_scene_modified ? "Save Scene *" : "Save Scene";
            if (ImGui::MenuItem(save_label.c_str(), "Ctrl+S", false, has_project)) {
                SaveScene();
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, has_project)) {
                SaveSceneAs();
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // Request engine exit
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            std::string undo_label = CanUndo() ? "Undo " + GetUndoDescription() : "Undo";
            std::string redo_label = CanRedo() ? "Redo " + GetRedoDescription() : "Redo";
            
            if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, CanUndo())) {
                Undo();
            }
            if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, CanRedo())) {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            if (ImGui::MenuItem("Delete", "Del")) {
                if (m_selected_node_id != 0 && m_selected_node_id != m_scene_root.id) {
                    DeleteNode(m_selected_node_id);
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Viewport", nullptr, &m_viewport_panel->visible);
            ImGui::MenuItem("Multi-Viewport", nullptr, &m_multi_viewport_panel->visible);
            ImGui::Separator();
            ImGui::MenuItem("Scene Tree", nullptr, &m_scene_tree_panel->visible);
            ImGui::MenuItem("Inspector", nullptr, &m_inspector_panel->visible);
            ImGui::MenuItem("Console", nullptr, &m_console_panel->visible);
            ImGui::Separator();
            ImGui::MenuItem("Shader Graph", nullptr, &m_shader_graph_editor->visible);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &m_show_demo_window);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Node")) {
            if (ImGui::MenuItem("Add Node", "Ctrl+A")) {
                m_show_add_node_popup = true;
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Add 3D Node")) {
                if (ImGui::MenuItem("Cube")) {
                    AddNode("Cube");
                }
                if (ImGui::MenuItem("Sphere")) {
                    AddNode("Sphere");
                }
                if (ImGui::MenuItem("Plane")) {
                    AddNode("Plane");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Empty Node3D")) {
                    AddNode("Node3D");
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            
            // Prefab menu items
            bool has_selection = (m_selected_node_id != 0 && m_selected_node_id != m_scene_root.id);
            if (ImGui::MenuItem("Save as Prefab...", nullptr, false, has_selection)) {
                if (has_selection) {
                    EditorNode* selected = GetSelectedNode();
                    if (selected) {
                        strncpy(m_prefab_name_buffer, selected->name.c_str(), sizeof(m_prefab_name_buffer) - 1);
                        m_show_save_prefab_popup = true;
                    }
                }
            }
            if (ImGui::BeginMenu("Instantiate Prefab")) {
                const auto& prefabs = m_prefab_manager.GetAllPrefabs();
                if (prefabs.empty()) {
                    ImGui::TextDisabled("No prefabs loaded");
                } else {
                    for (const auto& [name, prefab] : prefabs) {
                        if (ImGui::MenuItem(name.c_str())) {
                            m_prefab_manager.InstantiatePrefab(prefab);
                        }
                    }
                }
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {}
            if (ImGui::MenuItem("Rename", "F2")) {}
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Project")) {
            bool has_project = HasActiveProject();
            
            // Show current project name
            if (has_project) {
                ImGui::TextDisabled("Active: %s", m_active_project->GetName().c_str());
                ImGui::Separator();
            }
            
            if (ImGui::MenuItem("Project Settings...", nullptr, false, has_project)) {
                // TODO: Show project settings dialog
            }
            if (ImGui::MenuItem("Save Project", nullptr, false, has_project)) {
                if (m_active_project && m_active_project->Save()) {
                    Log("Project saved", 0);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Project", nullptr, false, has_project)) {
                if (m_active_project) {
                    m_active_project->Close();
                    m_active_project.reset();
                    m_current_scene_path.clear();
                    m_scene_modified = false;
                    Log("Project closed", 0);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export...", nullptr, false, has_project)) {
                // TODO: Export project dialog
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Assets")) {
            if (ImGui::MenuItem("Import Model...", "Ctrl+I")) {
                // Open file dialog for model import
                std::string filter = "3D Models|*.obj;*.fbx;*.gltf;*.glb;*.blend;*.dae;*.3ds;*.stl;*.ply|"
                                    "Wavefront OBJ|*.obj|"
                                    "Autodesk FBX|*.fbx|"
                                    "glTF|*.gltf;*.glb|"
                                    "Blender|*.blend|"
                                    "Collada|*.dae|"
                                    "All Files|*.*";
                
                std::string filepath = m_platform->OpenFileDialog("Import 3D Model", filter, "");
                
                if (!filepath.empty()) {
                    Log("Starting async import of: " + filepath, 0);
                    m_pending_import_path = filepath;
                    m_hot_reloader.ImportFileAsync(filepath);
                }
            }
            
            // Show import progress indicator
            if (m_hot_reloader.IsImportPending()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Importing model...");
            }
            
            ImGui::Separator();
            bool watching = m_hot_reloader.IsWatching();
            if (ImGui::MenuItem(watching ? "Stop Hot Reload" : "Start Hot Reload")) {
                if (watching) {
                    m_hot_reloader.Stop();
                    Log("Hot reload stopped", 0);
                } else {
                    m_hot_reloader.Start();
                    Log("Hot reload started", 0);
                }
            }
            if (ImGui::MenuItem("Refresh All Assets", "F5")) {
                m_hot_reloader.RefreshAll();
                Log("Refreshing all watched assets...", 0);
            }
            ImGui::Separator();
            
            // Show watched directories
            ImGui::TextDisabled("Watch Directories:");
            const auto& watch_dirs = m_hot_reloader.GetWatchDirectories();
            if (watch_dirs.empty()) {
                ImGui::TextDisabled("  (none)");
            } else {
                for (const auto& [dir, recursive] : watch_dirs) {
                    std::string label = "  " + dir;
                    if (recursive) label += " (recursive)";
                    ImGui::TextDisabled("%s", label.c_str());
                }
            }
            
            ImGui::Separator();
            ImGui::TextDisabled("Watched: %zu files", m_hot_reloader.GetWatchedFileCount());
            ImGui::TextDisabled("Imported: %zu assets", m_hot_reloader.GetImportedAssetCount());
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) {}
            if (ImGui::MenuItem("About ActionEngine")) {}
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void Editor::DrawToolbar() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->WorkPos.x, 
                                    ImGui::GetMainViewport()->WorkPos.y + 19));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->WorkSize.x, 40));
    
    ImGuiWindowFlags toolbar_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;
    toolbar_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
    toolbar_flags |= ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    
    if (ImGui::Begin("##Toolbar", nullptr, toolbar_flags)) {
        // Transform mode buttons
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        
        // Toggle gizmo visibility
        bool gizmo_active = m_gizmo_panel->enabled;
        if (gizmo_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
        }
        if (ImGui::Button("Gizmo (G)")) {
            m_gizmo_panel->enabled = !m_gizmo_panel->enabled;
        }
        if (gizmo_active) {
            ImGui::PopStyleColor();
        }
        
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        
        // Play/Stop buttons - center them
        float center_x = ImGui::GetMainViewport()->WorkSize.x / 2.0f;
        ImGui::SetCursorPosX(center_x - 60);
        
        if (m_play_mode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Stop")) {
                m_play_mode = false;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("Play")) {
                m_play_mode = true;
            }
            ImGui::PopStyleColor();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {}
        
        ImGui::PopStyleVar();
    }
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void Editor::DrawAddNodePopup() {
    if (m_show_add_node_popup) {
        ImGui::OpenPopup("Add Node");
        m_show_add_node_popup = false;
    }
    
    if (ImGui::BeginPopupModal("Add Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a node type to add:");
        ImGui::Separator();
        
        if (ImGui::BeginChild("NodeTypes", ImVec2(300, 200))) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "3D Nodes");
            if (ImGui::Selectable("  Cube")) {
                AddNode("Cube");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("  Sphere")) {
                AddNode("Sphere");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("  Plane")) {
                AddNode("Plane");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("  Node3D (Empty)")) {
                AddNode("Node3D");
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Lights");
            if (ImGui::Selectable("  PointLight")) {
                AddNode("PointLight");
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Editor::DrawSavePrefabPopup() {
    if (m_show_save_prefab_popup) {
        ImGui::OpenPopup("Save as Prefab");
        m_show_save_prefab_popup = false;
    }
    
    if (ImGui::BeginPopupModal("Save as Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Show how many nodes are selected
        size_t selection_count = m_selected_node_ids.size();
        if (selection_count > 1) {
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), 
                "%zu nodes selected (will create a group prefab)", selection_count);
        } else {
            ImGui::Text("Save the selected node as a reusable prefab:");
        }
        ImGui::Separator();
        
        ImGui::Text("Prefab Name:");
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##PrefabName", m_prefab_name_buffer, sizeof(m_prefab_name_buffer));
        
        ImGui::Separator();
        
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (strlen(m_prefab_name_buffer) > 0 && !m_selected_node_ids.empty()) {
                std::shared_ptr<Prefab> prefab;
                
                if (m_selected_node_ids.size() == 1) {
                    // Single node - use existing behavior
                    EditorNode* selected = GetSelectedNode();
                    if (selected) {
                        prefab = m_prefab_manager.CreatePrefab(m_prefab_name_buffer, *selected);
                    }
                } else {
                    // Multiple nodes - create a group prefab
                    // Create a temporary group node containing all selected nodes
                    EditorNode group_node;
                    group_node.id = 0;
                    group_node.name = m_prefab_name_buffer;
                    group_node.type = "Node3D";
                    group_node.visible = true;
                    group_node.scale = {1, 1, 1};
                    
                    // Add copies of all selected nodes as children
                    for (u32 node_id : m_selected_node_ids) {
                        if (EditorNode* node = FindNode(node_id)) {
                            group_node.children.push_back(*node);
                        }
                    }
                    
                    prefab = m_prefab_manager.CreatePrefab(m_prefab_name_buffer, group_node);
                }
                
                if (prefab) {
                    // Save to file
                    std::string filename = std::string(m_prefab_name_buffer) + ".prefab";
                    m_prefab_manager.SavePrefab(prefab, filename);
                    
                    Log("Saved prefab: " + std::string(m_prefab_name_buffer) + 
                        " (" + std::to_string(m_selected_node_ids.size()) + " nodes)", 0);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

EditorNode* Editor::AddNode(const std::string& type, EditorNode* parent) {
    if (!parent) {
        parent = &m_scene_root;
    }
    
    // Create editor node
    EditorNode node;
    node.id = m_next_node_id++;
    node.type = type;
    node.visible = true;
    node.scale = {1, 1, 1};
    
    // Determine mesh and name based on type
    MeshHandle mesh{0};
    if (type == "Cube") {
        node.name = "Cube";
        mesh = m_cube_mesh;
    } else if (type == "Sphere") {
        node.name = "Sphere";
        mesh = m_sphere_mesh;
    } else if (type == "Plane") {
        node.name = "Plane";
        mesh = m_plane_mesh;
    } else if (type == "Node3D") {
        node.name = "Node3D";
    } else if (type == "PointLight") {
        node.name = "PointLight";
    } else {
        node.name = type;
    }
    
    node.mesh = mesh;
    
    // Create ECS entity for mesh nodes
    if (mesh.is_valid()) {
        Entity entity = m_ecs->CreateEntity();
        node.entity = entity;
        
        // Add transform component
        auto& transform = m_ecs->AddComponent<TransformComponent>(entity);
        transform.position = node.position;
        transform.rotation = quat::identity();
        transform.scale = node.scale;
        
        // Add render component
        auto& render = m_ecs->AddComponent<RenderComponent>(entity);
        render.mesh = mesh;
        render.visible = true;
        render.cast_shadow = true;
        
        // Add bounds component
        auto& bounds = m_ecs->AddComponent<BoundsComponent>(entity);
        if (MeshData* mesh_data = m_assets->GetMesh(mesh)) {
            bounds.local_bounds = mesh_data->bounds;
            bounds.world_bounds = AABB(
                mesh_data->bounds.min + node.position,
                mesh_data->bounds.max + node.position
            );
        }
        
        // Add tag component
        m_ecs->AddComponent<TagComponent>(entity, {
            .name = node.name,
            .tags = Tags::Prop | Tags::Dynamic
        });
        
        // Add to world manager for rendering
        WorldObject obj;
        obj.entity = entity;
        obj.position = node.position;
        obj.bounds = bounds.world_bounds;
        obj.mesh = mesh;
        obj.color = vec4{node.color.x, node.color.y, node.color.z, 1.0f};
        obj.visible = true;
        obj.lod_level = 0;
        m_world->AddObject(obj);
        
        LOG_INFO("Created {} entity: {}", type, node.name);
    }
    
    // Add to parent's children
    parent->children.push_back(node);
    
    // Select the new node
    m_selected_node_id = node.id;
    
    return &parent->children.back();
}

void Editor::DeleteNode(u32 node_id) {
    if (node_id == 0 || node_id == m_scene_root.id) return;
    
    // Recursive function to find and delete
    std::function<bool(EditorNode&)> delete_from_parent = [&](EditorNode& parent) -> bool {
        for (auto it = parent.children.begin(); it != parent.children.end(); ++it) {
            if (it->id == node_id) {
                // Destroy ECS entity if exists
                if (it->entity != INVALID_ENTITY) {
                    m_ecs->DestroyEntity(it->entity);
                }
                parent.children.erase(it);
                return true;
            }
            if (delete_from_parent(*it)) {
                return true;
            }
        }
        return false;
    };
    
    if (delete_from_parent(m_scene_root)) {
        m_selected_node_id = 0;
        LOG_INFO("Deleted node {}", node_id);
    }
}

void Editor::SetSelectedNode(u32 node_id) {
    m_selected_node_id = node_id;
    // Update multi-select to match single selection
    m_selected_node_ids.clear();
    if (node_id != 0) {
        m_selected_node_ids.push_back(node_id);
    }
}

bool Editor::IsNodeSelected(u32 node_id) const {
    return std::find(m_selected_node_ids.begin(), m_selected_node_ids.end(), node_id) 
           != m_selected_node_ids.end();
}

EditorNode* Editor::GetSelectedNode() {
    if (m_selected_node_id == 0) return nullptr;
    
    // Search recursively
    std::function<EditorNode*(EditorNode&)> find_node = [&](EditorNode& node) -> EditorNode* {
        if (node.id == m_selected_node_id) return &node;
        for (auto& child : node.children) {
            if (auto* found = find_node(child)) return found;
        }
        return nullptr;
    };
    
    return find_node(m_scene_root);
}

void Editor::Log(const std::string& message, int level) {
    if (m_console_panel) {
        m_console_panel->AddMessage(message, level);
    }
}

bool Editor::WantsKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Editor::WantsMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool Editor::IsGizmoManipulating() const {
    return m_gizmo_panel && m_gizmo_panel->IsManipulating();
}

void Editor::SyncTransforms() {
    // Recursively sync all nodes with entities to ECS and WorldManager
    std::function<void(EditorNode&)> sync_node = [&](EditorNode& node) {
        if (node.entity != INVALID_ENTITY && m_ecs && m_world) {
            // Update ECS TransformComponent
            if (m_ecs->HasComponent<TransformComponent>(node.entity)) {
                auto* transform = m_ecs->GetComponent<TransformComponent>(node.entity);
                transform->position = node.position;
                transform->scale = node.scale;
                
                // Convert euler angles (degrees) to quaternion
                float rx = node.rotation.x * (PI / 180.0f);
                float ry = node.rotation.y * (PI / 180.0f);
                float rz = node.rotation.z * (PI / 180.0f);
                transform->rotation = quat::from_euler(rx, ry, rz);
            }
            
            // Update WorldManager object position and color
            vec4 color4 = vec4{node.color.x, node.color.y, node.color.z, 1.0f};
            m_world->UpdateObject(node.entity, node.position, color4);
        }
        
        // Recurse into children
        for (auto& child : node.children) {
            sync_node(child);
        }
    };
    
    sync_node(m_scene_root);
}

// ============================================================================
// Node Finding (for commands)
// ============================================================================

EditorNode* Editor::FindNode(u64 node_id) {
    return FindNode(node_id, m_scene_root);
}

EditorNode* Editor::FindNode(u64 node_id, EditorNode& root) {
    if (root.id == node_id) return &root;
    for (auto& child : root.children) {
        if (auto* found = FindNode(node_id, child)) return found;
    }
    return nullptr;
}

// ============================================================================
// Node Modification (for commands)
// ============================================================================

void Editor::SetNodeTransform(u64 node_id, const vec3& position, const vec3& rotation, const vec3& scale) {
    EditorNode* node = FindNode(node_id);
    if (node) {
        node->position = position;
        node->rotation = rotation;
        node->scale = scale;
    }
}

void Editor::SetNodePosition(u64 node_id, const vec3& position) {
    EditorNode* node = FindNode(node_id);
    if (node) {
        node->position = position;
    }
}

void Editor::SetNodeRotation(u64 node_id, const vec3& rotation) {
    EditorNode* node = FindNode(node_id);
    if (node) {
        node->rotation = rotation;
    }
}

void Editor::SetNodeScale(u64 node_id, const vec3& scale) {
    EditorNode* node = FindNode(node_id);
    if (node) {
        node->scale = scale;
    }
}

void Editor::SetNodeColor(u64 node_id, const vec3& color) {
    EditorNode* node = FindNode(node_id);
    if (node) {
        node->color = color;
    }
}

void Editor::SetNodeName(u64 node_id, const std::string& name) {
    EditorNode* node = FindNode(node_id);
    if (node) {
        node->name = name;
    }
}

void Editor::SetNodeParent(u64 node_id, u64 new_parent_id) {
    // This is complex - need to remove from old parent and add to new parent
    // For now, just log (full implementation would require more careful tree manipulation)
    LOG_INFO("Reparent node {} to {}", node_id, new_parent_id);
}

// ============================================================================
// Command Execution
// ============================================================================

void Editor::ExecuteCommand(std::unique_ptr<Command> command) {
    m_command_history.Execute(std::move(command));
}

bool Editor::Undo() {
    if (m_command_history.CanUndo()) {
        std::string desc = m_command_history.GetUndoDescription();
        bool result = m_command_history.Undo();
        if (result) {
            LOG_INFO("Undo: {}", desc);
        }
        return result;
    }
    return false;
}

bool Editor::Redo() {
    if (m_command_history.CanRedo()) {
        std::string desc = m_command_history.GetRedoDescription();
        bool result = m_command_history.Redo();
        if (result) {
            LOG_INFO("Redo: {}", desc);
        }
        return result;
    }
    return false;
}

// ============================================================================
// Project Management
// ============================================================================

void Editor::DrawNewProjectPopup() {
    if (m_show_new_project_popup) {
        ImGui::OpenPopup("New Project");
    }
    
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("New Project", &m_show_new_project_popup, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Create a new ActionEngine project");
        ImGui::Separator();
        
        ImGui::InputText("Project Name", m_new_project_name, sizeof(m_new_project_name));
        
        ImGui::InputText("Location", m_new_project_path, sizeof(m_new_project_path));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            std::string dir = m_platform->OpenFolderDialog("Select Project Location", m_new_project_path);
            if (!dir.empty()) {
                strncpy(m_new_project_path, dir.c_str(), sizeof(m_new_project_path) - 1);
            }
        }
        
        // Show full project path
        std::string full_path = std::string(m_new_project_path) + "/" + std::string(m_new_project_name);
        ImGui::TextDisabled("Project will be created at: %s", full_path.c_str());
        
        ImGui::Separator();
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(m_new_project_name) > 0 && strlen(m_new_project_path) > 0) {
                auto project = Project::Create(m_new_project_name, m_new_project_path);
                if (project) {
                    // Clear existing scene content
                    for (auto& child : m_scene_root.children) {
                        if (child.entity != INVALID_ENTITY && m_ecs->IsAlive(child.entity)) {
                            m_ecs->DestroyEntity(child.entity);
                        }
                    }
                    m_scene_root.children.clear();
                    m_selected_node_id = 0;
                    m_selected_node_ids.clear();
                    
                    m_active_project = std::move(project);
                    m_current_scene_path.clear();
                    m_scene_modified = false;
                    Log("Created project: " + std::string(m_new_project_name), 0);
                    m_show_new_project_popup = false;
                    ImGui::CloseCurrentPopup();
                } else {
                    Log("Failed to create project", 2);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_new_project_popup = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

bool Editor::NewProject() {
    m_show_new_project_popup = true;
    return true;
}

bool Editor::OpenProject() {
    std::string filter = "ActionEngine Project|*.aeproj|All Files|*.*";
    std::string filepath = m_platform->OpenFileDialog("Open Project", filter, "");
    
    if (!filepath.empty()) {
        return OpenProject(filepath);
    }
    return false;
}

bool Editor::OpenProject(const std::string& path) {
    auto project = Project::Open(path);
    if (project) {
        // Close current project if any
        if (m_active_project) {
            m_active_project->Close();
        }
        
        // Clear existing scene content before loading new project
        for (auto& child : m_scene_root.children) {
            if (child.entity != INVALID_ENTITY && m_ecs->IsAlive(child.entity)) {
                m_ecs->DestroyEntity(child.entity);
            }
        }
        m_scene_root.children.clear();
        m_selected_node_id = 0;
        m_selected_node_ids.clear();
        
        m_active_project = std::move(project);
        m_current_scene_path.clear();
        m_scene_modified = false;
        
        Log("Opened project: " + m_active_project->GetName(), 0);
        
        // Load default scene if available
        const auto& scenes = m_active_project->GetScenes();
        if (!scenes.empty()) {
            std::string scene_path = m_active_project->GetScenePath(scenes[0]);
            if (!scene_path.empty()) {
                LoadScene(scene_path);
            }
        }
        
        return true;
    }
    
    Log("Failed to open project: " + path, 2);
    return false;
}

bool Editor::SaveScene() {
    if (!m_active_project) {
        Log("No active project - create or open a project first", 1);
        return false;
    }
    
    if (m_current_scene_path.empty()) {
        return SaveSceneAs();
    }
    
    SceneSerializer serializer(*m_ecs, *this);
    if (serializer.SaveScene(m_current_scene_path)) {
        m_scene_modified = false;
        Log("Saved scene: " + m_current_scene_path, 0);
        return true;
    }
    
    Log("Failed to save scene", 2);
    return false;
}

bool Editor::SaveSceneAs() {
    if (!m_active_project) {
        Log("No active project - create or open a project first", 1);
        return false;
    }
    
    std::string filter = "Scene Files|*.aescene|All Files|*.*";
    std::string scenes_dir = m_active_project->GetScenesPath();
    std::string filepath = m_platform->SaveFileDialog("Save Scene As", filter, scenes_dir);
    
    if (!filepath.empty()) {
        // Ensure .aescene extension
        if (filepath.find(".aescene") == std::string::npos) {
            filepath += ".aescene";
        }
        
        SceneSerializer serializer(*m_ecs, *this);
        if (serializer.SaveScene(filepath)) {
            m_current_scene_path = filepath;
            m_scene_modified = false;
            
            // Add to project's scene list
            std::string scene_name = filepath.substr(filepath.find_last_of("/\\") + 1);
            scene_name = scene_name.substr(0, scene_name.find(".aescene"));
            m_active_project->AddScene(scene_name);
            m_active_project->Save();
            
            Log("Saved scene: " + filepath, 0);
            return true;
        }
    }
    
    return false;
}

bool Editor::LoadScene(const std::string& path) {
    if (!m_active_project) {
        Log("No active project", 1);
        return false;
    }
    
    SceneSerializer serializer(*m_ecs, *this);
    if (serializer.LoadScene(path)) {
        m_current_scene_path = path;
        m_scene_modified = false;
        Log("Loaded scene: " + path, 0);
        return true;
    }
    
    Log("Failed to load scene: " + path, 2);
    return false;
}

} // namespace action
