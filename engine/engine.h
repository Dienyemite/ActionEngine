#pragma once

/*
 * ActionEngine - Seamless Action-Adventure Game Engine
 * 
 * Target Hardware: GTX 660 (2GB VRAM) + i5-6600K (4 cores)
 * Target Performance: 1080p @ 60 FPS
 * 
 * Key Features:
 * - Forward+ clustered rendering (Vulkan 1.0)
 * - Seamless world streaming (no loading screens)
 * - Aggressive LOD system (5 levels)
 * - Predictive asset loading
 * - Data-oriented ECS
 * - 4-thread job system
 */

#include "core/types.h"
#include "core/memory/allocators.h"
#include "core/jobs/job_system.h"
#include "platform/platform.h"
#include "render/renderer.h"
#include "world/world_manager.h"
#include "assets/asset_manager.h"
#include "gameplay/ecs/ecs.h"
#include "scripting/script_system.h"
#include "editor/editor.h"

namespace action {

// Engine configuration with hardware-specific defaults
struct EngineConfig {
    // Window settings
    uint32_t window_width = 1920;
    uint32_t window_height = 1080;
    bool fullscreen = false;
    bool vsync = true;
    
    // Performance budgets (GTX 660 + i5-6600K targets)
    struct PerformanceBudgets {
        size_t vram_budget = 1600 * 1024 * 1024;        // 1.6 GB
        size_t texture_pool_size = 800 * 1024 * 1024;   // 800 MB
        size_t mesh_pool_size = 300 * 1024 * 1024;      // 300 MB
        size_t upload_per_frame = 2 * 1024 * 1024;      // 2 MB/frame
        uint32_t max_draw_calls = 2500;
        uint32_t max_triangles = 800000;
        float target_frame_time_ms = 16.667f;           // 60 FPS
    } budgets;
    
    // Quality settings
    struct QualitySettings {
        uint32_t shadow_cascade_count = 2;
        uint32_t shadow_resolution = 1024;
        float lod_bias = 1.0f;
        float draw_distance = 400.0f;
        uint32_t max_texture_size = 1024;
        uint32_t anisotropic_level = 4;
        bool bloom_enabled = true;
        float particle_density = 0.75f;
    } quality;
    
    // Streaming settings
    struct StreamingSettings {
        float prediction_time = 2.0f;           // Look-ahead seconds
        float hysteresis_distance = 10.0f;      // Prevent thrashing
        uint32_t hot_zone_radius = 100;         // Meters
        uint32_t warm_zone_radius = 500;
        uint32_t cold_zone_radius = 2000;
    } streaming;
    
    // Threading (4-core target)
    uint32_t worker_thread_count = 3;   // Main + 3 workers
};

// Game update callback type
using GameUpdateCallback = std::function<void(float dt)>;

class Engine {
public:
    static Engine& Get();
    
    bool Initialize(const EngineConfig& config);
    void Shutdown();
    
    // Register a game update callback (called each frame before ECS update)
    void SetGameUpdateCallback(GameUpdateCallback callback) { m_game_update_callback = callback; }
    
    void Run();
    void RequestExit();
    
    // Subsystem access
    Platform& GetPlatform() { return *m_platform; }
    Renderer& GetRenderer() { return *m_renderer; }
    WorldManager& GetWorld() { return *m_world; }
    AssetManager& GetAssets() { return *m_assets; }
    JobSystem& GetJobs() { return *m_jobs; }
    ECS& GetECS() { return *m_ecs; }
    ScriptSystem& GetScripts() { return *m_scripts; }
    Editor& GetEditor() { return *m_editor; }
    
    // Frame timing
    float GetDeltaTime() const { return m_delta_time; }
    float GetTotalTime() const { return m_total_time; }
    uint64_t GetFrameNumber() const { return m_frame_number; }
    
    // Performance monitoring
    struct FrameStats {
        float frame_time_ms;
        float cpu_time_ms;
        float gpu_time_ms;
        uint32_t draw_calls;
        uint32_t triangles;
        size_t vram_used;
        size_t streaming_uploaded;
    };
    const FrameStats& GetFrameStats() const { return m_frame_stats; }
    
private:
    Engine() = default;
    ~Engine() = default;
    
    void MainLoop();
    void Update(float dt);
    void Render();
    void UpdateStats();
    
    EngineConfig m_config;
    bool m_running = false;
    
    // Timing
    float m_delta_time = 0.0f;
    float m_total_time = 0.0f;
    uint64_t m_frame_number = 0;
    
    // Subsystems (order matters for initialization/shutdown)
    std::unique_ptr<Platform> m_platform;
    std::unique_ptr<JobSystem> m_jobs;
    std::unique_ptr<AssetManager> m_assets;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<WorldManager> m_world;
    std::unique_ptr<ECS> m_ecs;
    std::unique_ptr<ScriptSystem> m_scripts;
    std::unique_ptr<Editor> m_editor;
    
    // Game callback
    GameUpdateCallback m_game_update_callback;
    
    FrameStats m_frame_stats{};
};

} // namespace action
