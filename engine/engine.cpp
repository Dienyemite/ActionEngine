#include "engine.h"
#include "core/logging.h"
#include "core/profiler.h"
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
    
    // 7. Editor
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
    
    // ========================================
    // Camera Controls (Blender/Godot style)
    // - Right-click drag: Orbit/rotate around pivot
    // - Middle-click drag: Pan camera
    // - Scroll wheel: Zoom in/out
    // ========================================
    
    // Camera orbit parameters (persistent state - outside conditional so always accessible)
    static vec3 pivot_point{0, 0, 0};       // Point camera orbits around
    static float orbit_distance = 10.0f;     // Distance from pivot
    static float orbit_yaw = 0.0f;           // Horizontal angle
    static float orbit_pitch = 0.3f;         // Vertical angle (start slightly above)
    
    Input& input = m_platform->GetInput();
    Camera& camera = m_renderer->GetCamera();
    const MouseState& mouse = input.GetMouse();
    
    // Check if gizmo is being manipulated - don't process camera input then
    bool gizmo_active = m_editor->IsGizmoManipulating();
    
    // Process camera input only when:
    // 1. Gizmo is not being manipulated
    // 2. Right-click/middle-click for orbit/pan (always allow these)
    // 3. Or when ImGui doesn't want input for other controls
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
            float pan_speed = 0.01f * orbit_distance;  // Scale with zoom level
            
            // Calculate right and up vectors in world space
            vec3 cam_forward = camera.forward;
            vec3 cam_right = vec3{cam_forward.z, 0, -cam_forward.x}.normalized();
            vec3 cam_up = camera.up;
            
            // Pan the pivot point
            pivot_point = pivot_point - cam_right * mouse.delta_x * pan_speed;
            pivot_point = pivot_point + cam_up * mouse.delta_y * pan_speed;
        }
        
        // Scroll wheel: Zoom in/out
        if (mouse.scroll_delta != 0) {
            float zoom_speed = 0.15f;
            orbit_distance *= (1.0f - mouse.scroll_delta * zoom_speed);
            
            // Clamp zoom distance
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
    
    // ALWAYS update camera position from orbit parameters (so gizmo always has correct view)
    camera.forward = vec3{
        std::cos(orbit_pitch) * std::sin(orbit_yaw),
        std::sin(orbit_pitch),
        std::cos(orbit_pitch) * std::cos(orbit_yaw)
    }.normalized();
    
    camera.position = pivot_point - camera.forward * orbit_distance;
    
    // Update input state for next frame
    m_platform->GetInput().Update();
    
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
    
    // Execute pending jobs
    {
        PROFILE_SCOPE("Jobs::Execute");
        m_jobs->ExecuteMainThreadJobs();
    }
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
    
    // Warn if over budget
    if (m_frame_stats.frame_time_ms > m_config.budgets.target_frame_time_ms * 1.1f) {
        LOG_WARN("Frame time exceeded budget: {:.2f}ms > {:.2f}ms",
                 m_frame_stats.frame_time_ms, 
                 m_config.budgets.target_frame_time_ms);
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
