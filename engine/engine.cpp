#include "engine.h"
#include "core/logging.h"
#include "core/profiler.h"
#include "scripting/script_system.h"
#include "scripting/builtin_scripts.h"
#include <chrono>
#include <cmath>

namespace action {

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

bool Engine::Initialize(const EngineConfig& config) {
    m_config = config;
    
    LOG_INFO("ActionEngine initializing...");
    LOG_INFO("Target: 1080p @ 60 FPS, VRAM budget: {} MB", 
             config.budgets.vram_budget / (1024 * 1024));
    
    // Initialize subsystems in order
    
    // 1. Platform (window, input)
    m_platform = std::make_unique<Platform>();
    if (!m_platform->Initialize(config.window_width, config.window_height, 
                                 "ActionEngine", config.fullscreen)) {
        LOG_ERROR("Failed to initialize platform");
        return false;
    }
    
    // 2. Job system (4 threads for i5-6600K)
    m_jobs = std::make_unique<JobSystem>();
    if (!m_jobs->Initialize(config.worker_thread_count)) {
        LOG_ERROR("Failed to initialize job system");
        return false;
    }
    
    // 3. Asset manager (streaming system)
    m_assets = std::make_unique<AssetManager>();
    AssetManagerConfig asset_config{
        .texture_pool_size = config.budgets.texture_pool_size,
        .mesh_pool_size = config.budgets.mesh_pool_size,
        .upload_budget_per_frame = config.budgets.upload_per_frame,
        .prediction_time = config.streaming.prediction_time,
    };
    if (!m_assets->Initialize(asset_config)) {
        LOG_ERROR("Failed to initialize asset manager");
        return false;
    }
    
    // 4. Renderer (Forward+ Vulkan)
    m_renderer = std::make_unique<Renderer>();
    RendererConfig render_config{
        .window = m_platform->GetWindowHandle(),
        .width = config.window_width,
        .height = config.window_height,
        .vsync = config.vsync,
        .vram_budget = config.budgets.vram_budget,
        .max_draw_calls = config.budgets.max_draw_calls,
        .shadow_cascade_count = config.quality.shadow_cascade_count,
        .shadow_resolution = config.quality.shadow_resolution,
    };
    if (!m_renderer->Initialize(render_config)) {
        LOG_ERROR("Failed to initialize renderer");
        return false;
    }
    
    // Connect asset manager to Vulkan context for GPU uploads
    m_assets->SetVulkanContext(&m_renderer->GetContext());
    
    // Connect renderer to asset manager for mesh access
    m_renderer->SetAssetManager(m_assets.get());
    
    // 5. World manager (streaming, chunks)
    m_world = std::make_unique<WorldManager>();
    WorldManagerConfig world_config{
        .chunk_size = 256.0f,
        .hot_zone_radius = static_cast<float>(config.streaming.hot_zone_radius),
        .warm_zone_radius = static_cast<float>(config.streaming.warm_zone_radius),
        .cold_zone_radius = static_cast<float>(config.streaming.cold_zone_radius),
        .lod_bias = config.quality.lod_bias,
        .draw_distance = config.quality.draw_distance,
    };
    if (!m_world->Initialize(world_config)) {
        LOG_ERROR("Failed to initialize world manager");
        return false;
    }
    
    // 6. ECS
    m_ecs = std::make_unique<ECS>();
    if (!m_ecs->Initialize()) {
        LOG_ERROR("Failed to initialize ECS");
        return false;
    }
    
    // Connect WorldManager to ECS for transform queries
    m_world->SetECS(m_ecs.get());
    
    // 7. Physics World (legacy - spatial queries)
    m_physics = std::make_unique<PhysicsWorld>();
    if (!m_physics->Initialize(m_ecs.get(), 4.0f)) {
        LOG_ERROR("Failed to initialize PhysicsWorld");
        return false;
    }
    
    // 7b. Jolt Physics (rigidbody simulation)
    m_jolt_physics = std::make_unique<JoltPhysics>();
    JoltPhysicsConfig jolt_config;
    jolt_config.gravity = {0, -25.0f, 0};  // Higher gravity for weighty feel
    jolt_config.num_threads = config.worker_thread_count;
    if (!m_jolt_physics->Initialize(m_ecs.get(), jolt_config)) {
        LOG_ERROR("Failed to initialize JoltPhysics");
        return false;
    }
    
    // 8. Character Controller
    m_character_controller = std::make_unique<CharacterController>();
    m_character_controller->Initialize(m_physics.get(), m_ecs.get());
    
    // 9. Script System
    m_scripts = std::make_unique<ScriptSystem>();
    m_scripts->Initialize(m_ecs.get(), &m_platform->GetInput(), 
                           m_assets.get(), m_world.get(), m_renderer.get(),
                           m_physics.get());
    
    // 10. Editor
    m_editor = std::make_unique<Editor>();
    EditorConfig editor_config{};
    editor_config.dark_theme = true;
    editor_config.start_in_play_mode = false;
    if (!m_editor->Initialize(*m_renderer, *m_platform, *m_ecs, *m_assets, *m_world, editor_config)) {
        LOG_ERROR("Failed to initialize Editor");
        return false;
    }
    
    // Set up UI render callback for editor
    m_renderer->SetUIRenderCallback([this](VkCommandBuffer cmd) {
        m_editor->Render(cmd);
    });
    
    LOG_INFO("ActionEngine initialized successfully");
    m_running = true;
    return true;
}

void Engine::Shutdown() {
    LOG_INFO("ActionEngine shutting down...");
    
    // Shutdown in reverse order
    // Editor must shutdown before Renderer
    if (m_editor) m_editor->Shutdown();
    // Scripts must shutdown before ECS
    if (m_scripts) m_scripts->Shutdown();
    // Physics (Jolt first, then legacy)
    if (m_jolt_physics) m_jolt_physics->Shutdown();
    if (m_physics) m_physics->Shutdown();
    // AssetManager must shutdown before Renderer (which owns VulkanContext)
    if (m_ecs) m_ecs->Shutdown();
    if (m_world) m_world->Shutdown();
    if (m_assets) m_assets->Shutdown();  // Clean up GPU resources first
    if (m_renderer) m_renderer->Shutdown();  // Then destroy Vulkan context
    if (m_jobs) m_jobs->Shutdown();
    if (m_platform) m_platform->Shutdown();
    
    LOG_INFO("ActionEngine shutdown complete");
}

void Engine::Run() {
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (m_running) {
        PROFILE_SCOPE("Frame");
        
        // Calculate delta time
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta = current_time - last_time;
        last_time = current_time;
        
        m_delta_time = delta.count();
        m_total_time += m_delta_time;
        m_frame_number++;
        
        // Cap delta time to prevent spiral of death
        if (m_delta_time > 0.1f) {
            m_delta_time = 0.1f;
        }
        
        MainLoop();
    }
}

void Engine::MainLoop() {
    // Process platform events (input, window)
    {
        PROFILE_SCOPE("Platform::Update");
        m_platform->PollEvents();
        
        if (m_platform->ShouldClose()) {
            RequestExit();
            return;
        }
    }
    
    // Update game logic
    Update(m_delta_time);
    
    // Render frame
    Render();
    
    // Update performance stats
    UpdateStats();
}

void Engine::Update(float dt) {
    PROFILE_SCOPE("Update");
    
    // Begin editor frame (for ImGui input)
    m_editor->BeginFrame();
    
    Input& input = m_platform->GetInput();
    Camera& camera = m_renderer->GetCamera();
    const MouseState& mouse = input.GetMouse();
    
    // F5 toggles play mode (works in both modes)
    if (input.IsKeyPressed(Key::F5)) {
        m_editor->TogglePlayMode();
        if (m_editor->IsPlayMode()) {
            LOG_INFO("Entered PLAY MODE - Use WASD to move, Space to jump");
        } else {
            LOG_INFO("Exited to EDITOR MODE");
        }
    }
    
    // Check if we're in play mode
    bool play_mode = m_editor->IsPlayMode();
    
    // ========================================
    // PLAY MODE: Third-person camera follows player
    // ========================================
    if (play_mode) {
        // Third-person camera parameters (persistent state)
        static float cam_distance = 8.0f;         // Distance behind player
        static float cam_height = 3.0f;           // Height above player
        static float cam_yaw = 0.0f;              // Horizontal angle (controlled by mouse)
        static float cam_pitch = 0.3f;            // Vertical angle (controlled by mouse)
        static float cam_sensitivity = 0.003f;
        static vec3 smooth_target{0, 0, 0};       // Smoothed camera target
        
        // Mouse look (when right-click held, or always during play)
        if (input.IsKeyDown(Key::MouseRight)) {
            cam_yaw -= mouse.delta_x * cam_sensitivity;
            cam_pitch -= mouse.delta_y * cam_sensitivity;
            
            // Clamp pitch to prevent flipping
            if (cam_pitch > 1.2f) cam_pitch = 1.2f;
            if (cam_pitch < -0.3f) cam_pitch = -0.3f;
        }
        
        // Scroll to adjust camera distance
        if (mouse.scroll_delta != 0) {
            cam_distance -= mouse.scroll_delta * 0.5f;
            if (cam_distance < 2.0f) cam_distance = 2.0f;
            if (cam_distance > 20.0f) cam_distance = 20.0f;
        }
        
        // Get player position
        Entity player_entity = m_ecs->GetPlayerEntity();
        if (player_entity != INVALID_ENTITY) {
            auto* player_transform = m_ecs->GetComponent<TransformComponent>(player_entity);
            if (player_transform) {
                // Target is slightly above player (shoulder level)
                vec3 target = player_transform->position + vec3{0, 1.5f, 0};
                
                // Smooth camera target movement
                float smooth_factor = 10.0f * dt;
                if (smooth_factor > 1.0f) smooth_factor = 1.0f;
                smooth_target = smooth_target + (target - smooth_target) * smooth_factor;
                
                // Calculate camera offset from target based on yaw/pitch
                vec3 offset{
                    std::cos(cam_pitch) * std::sin(cam_yaw) * cam_distance,
                    std::sin(cam_pitch) * cam_distance + cam_height,
                    std::cos(cam_pitch) * std::cos(cam_yaw) * cam_distance
                };
                
                camera.position = smooth_target + offset;
                camera.forward = (smooth_target - camera.position).normalized();
                
                // Update PlayerController's camera_yaw for movement direction
                auto* script_comp = m_ecs->GetComponent<ScriptComponent>(player_entity);
                if (script_comp) {
                    for (auto& script : script_comp->scripts) {
                        // Try to find PlayerController and update its camera_yaw
                        if (script->GetTypeName() == "PlayerController") {
                            // Use reflection or direct access - for now use a simple cast
                            // We pass yaw in degrees for the script
                            auto* player_ctrl = static_cast<PlayerController*>(script.get());
                            player_ctrl->camera_yaw = cam_yaw * RAD_TO_DEG;
                        }
                    }
                }
            }
        }
        
        // Escape to exit play mode
        if (input.IsKeyPressed(Key::Escape)) {
            m_editor->SetPlayMode(false);
        }
    }
    // ========================================
    // EDITOR MODE: Blender/Godot style camera
    // ========================================
    else {
        // Camera orbit parameters (persistent state)
        static vec3 pivot_point{0, 0, 0};
        static float orbit_distance = 10.0f;
        static float orbit_yaw = 0.0f;
        static float orbit_pitch = 0.3f;
        
        // Check if gizmo is being manipulated
        bool gizmo_active = m_editor->IsGizmoManipulating();
        
        if (!gizmo_active) {
            // Escape to exit
            if (input.IsKeyPressed(Key::Escape)) {
                RequestExit();
            }
            
            // Right-click drag: Orbit camera around pivot
            if (input.IsKeyDown(Key::MouseRight)) {
                float sensitivity = 0.005f;
                orbit_yaw -= mouse.delta_x * sensitivity;
                orbit_pitch -= mouse.delta_y * sensitivity;
                
                // Clamp pitch to prevent flipping
                if (orbit_pitch > 1.5f) orbit_pitch = 1.5f;
                if (orbit_pitch < -1.5f) orbit_pitch = -1.5f;
            }
            
            // Middle-click drag: Pan camera (move pivot point)
            if (input.IsKeyDown(Key::MouseMiddle)) {
                float pan_speed = 0.01f * orbit_distance;
                
                vec3 cam_forward = camera.forward;
                vec3 cam_right = vec3{cam_forward.z, 0, -cam_forward.x}.normalized();
                vec3 cam_up = camera.up;
                
                pivot_point = pivot_point - cam_right * mouse.delta_x * pan_speed;
                pivot_point = pivot_point + cam_up * mouse.delta_y * pan_speed;
            }
            
            // Scroll wheel: Zoom in/out
            if (mouse.scroll_delta != 0) {
                float zoom_speed = 0.15f;
                orbit_distance *= (1.0f - mouse.scroll_delta * zoom_speed);
                
                if (orbit_distance < 0.5f) orbit_distance = 0.5f;
                if (orbit_distance > 500.0f) orbit_distance = 500.0f;
            }
            
            // WASD for moving pivot (when not typing in UI)
            if (!m_editor->WantsKeyboard()) {
                float move_speed = 10.0f;
                if (input.IsKeyDown(Key::Shift)) {
                    move_speed = 30.0f;
                }
                
                vec3 cam_forward_flat = vec3{camera.forward.x, 0, camera.forward.z}.normalized();
                vec3 cam_right = vec3{camera.forward.z, 0, -camera.forward.x}.normalized();
                
                if (input.IsKeyDown(Key::W)) pivot_point = pivot_point + cam_forward_flat * move_speed * dt;
                if (input.IsKeyDown(Key::S)) pivot_point = pivot_point - cam_forward_flat * move_speed * dt;
                if (input.IsKeyDown(Key::A)) pivot_point = pivot_point - cam_right * move_speed * dt;
                if (input.IsKeyDown(Key::D)) pivot_point = pivot_point + cam_right * move_speed * dt;
                if (input.IsKeyDown(Key::E) || input.IsKeyDown(Key::Space)) pivot_point.y += move_speed * dt;
                if (input.IsKeyDown(Key::Q)) pivot_point.y -= move_speed * dt;
                
                // Focus on selected object with F key
                if (input.IsKeyPressed(Key::F)) {
                    EditorNode* selected = m_editor->GetSelectedNode();
                    if (selected && selected->entity != INVALID_ENTITY) {
                        pivot_point = selected->position;
                    }
                }
                
                // Undo/Redo shortcuts (Ctrl+Z / Ctrl+Y)
                if (input.IsKeyDown(Key::Control)) {
                    if (input.IsKeyPressed(Key::Z)) {
                        m_editor->Undo();
                    }
                    if (input.IsKeyPressed(Key::Y)) {
                        m_editor->Redo();
                    }
                }
            }
        }
        
        // Update camera position from orbit parameters
        camera.forward = vec3{
            std::cos(orbit_pitch) * std::sin(orbit_yaw),
            std::sin(orbit_pitch),
            std::cos(orbit_pitch) * std::cos(orbit_yaw)
        }.normalized();
        
        camera.position = pivot_point - camera.forward * orbit_distance;
    }
    
    // Update editor UI
    m_editor->Update(dt);
    
    // Get player position for streaming
    vec3 player_pos = m_ecs->GetPlayerPosition();
    vec3 player_velocity = m_ecs->GetPlayerVelocity();
    
    // Update world streaming (predictive loading)
    {
        PROFILE_SCOPE("World::Update");
        m_world->Update(player_pos, player_velocity, dt);
    }
    
    // Process asset streaming queue
    {
        PROFILE_SCOPE("Assets::Update");
        m_assets->Update(m_config.budgets.upload_per_frame);
    }
    
    // Call game update callback (for custom game logic like cube control)
    if (m_game_update_callback) {
        PROFILE_SCOPE("Game::Update");
        m_game_update_callback(dt);
    }
    
    // Update ECS systems
    {
        PROFILE_SCOPE("ECS::Update");
        m_ecs->Update(dt);
    }
    
    // Update physics (spatial hash, character controllers, Jolt simulation)
    {
        PROFILE_SCOPE("Physics::Update");
        m_physics->UpdateSpatialHash();
        m_character_controller->Update(dt);
        
        // Jolt Physics simulation
        m_jolt_physics->SyncToPhysics();   // Sync kinematic bodies to Jolt
        m_jolt_physics->Update(dt);         // Step simulation
        m_jolt_physics->SyncFromPhysics(); // Sync dynamic bodies back to ECS
    }
    
    // Update scripts (OnUpdate, LateUpdate)
    {
        PROFILE_SCOPE("Scripts::Update");
        m_scripts->Update(dt);
        m_scripts->LateUpdate(dt);
    }
    
    // Execute pending jobs
    {
        PROFILE_SCOPE("Jobs::Execute");
        m_jobs->ExecuteMainThreadJobs();
    }
    
    // Update input state for next frame (must be at end so scripts can see IsKeyPressed)
    m_platform->GetInput().Update();
}

void Engine::Render() {
    PROFILE_SCOPE("Render");
    
    // Gather visible objects from world
    RenderList render_list;
    {
        PROFILE_SCOPE("GatherRenderables");
        m_world->GatherVisibleObjects(m_renderer->GetCamera(), render_list);
    }
    
    // Submit to renderer
    {
        PROFILE_SCOPE("Renderer::Render");
        m_renderer->BeginFrame();
        m_renderer->RenderScene(render_list);
        m_renderer->EndFrame();
    }
    
    // End editor frame (handle viewports, etc.)
    m_editor->EndFrame();
}

void Engine::UpdateStats() {
    m_frame_stats.frame_time_ms = m_delta_time * 1000.0f;
    m_frame_stats.draw_calls = m_renderer->GetDrawCallCount();
    m_frame_stats.triangles = m_renderer->GetTriangleCount();
    m_frame_stats.vram_used = m_renderer->GetVRAMUsage();
    m_frame_stats.streaming_uploaded = m_assets->GetBytesUploadedThisFrame();
    
    // Warn if over budget (rate-limited to once per second)
    static float last_frame_warning_time = 0.0f;
    if (m_frame_stats.frame_time_ms > m_config.budgets.target_frame_time_ms * 1.1f) {
        float current_time = static_cast<float>(m_total_time);
        if (current_time - last_frame_warning_time > 1.0f) {
            LOG_WARN("Frame time exceeded budget: {:.2f}ms > {:.2f}ms",
                     m_frame_stats.frame_time_ms, 
                     m_config.budgets.target_frame_time_ms);
            last_frame_warning_time = current_time;
        }
    }
    
    if (m_frame_stats.draw_calls > m_config.budgets.max_draw_calls) {
        LOG_WARN("Draw calls exceeded budget: {} > {}",
                 m_frame_stats.draw_calls, 
                 m_config.budgets.max_draw_calls);
    }
}

void Engine::RequestExit() {
    m_running = false;
}

} // namespace action
