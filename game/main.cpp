#include "engine/engine.h"
#include "core/logging.h"
#include "core/math/math.h"

int main() {
    using namespace action;
    
    LOG_INFO("ActionEngine Starting...");
    
    // Configure engine for GTX 660 + i5-6600K target
    EngineConfig config;
    config.window_width = 1920;
    config.window_height = 1080;
    config.fullscreen = false;
    config.vsync = true;
    
    // Performance budgets for low-end hardware
    config.budgets.vram_budget = 1600_MB;
    config.budgets.texture_pool_size = 800_MB;
    config.budgets.mesh_pool_size = 300_MB;
    config.budgets.upload_per_frame = 2_MB;
    config.budgets.max_draw_calls = 2500;
    config.budgets.max_triangles = 800000;
    
    // Quality settings (medium preset)
    config.quality.shadow_cascade_count = 2;
    config.quality.shadow_resolution = 1024;
    config.quality.lod_bias = 1.0f;
    config.quality.draw_distance = 400.0f;
    config.quality.max_texture_size = 1024;
    config.quality.bloom_enabled = true;
    
    // Streaming settings
    config.streaming.hot_zone_radius = 100;
    config.streaming.warm_zone_radius = 500;
    config.streaming.cold_zone_radius = 2000;
    config.streaming.prediction_time = 2.0f;
    
    // Threading for 4-core CPU
    config.worker_thread_count = 3;
    
    // Initialize engine
    Engine& engine = Engine::Get();
    if (!engine.Initialize(config)) {
        LOG_FATAL("Failed to initialize engine");
        return 1;
    }
    
    // Get subsystems
    Renderer& renderer = engine.GetRenderer();
    
    // ========================================
    // Set up camera (default view)
    // ========================================
    Camera& camera = renderer.GetCamera();
    camera.position = {0, 5, -15};
    camera.forward = normalize(vec3{0, -0.2f, 1});
    camera.up = {0, 1, 0};
    camera.fov = Radians(75.0f);
    camera.near_plane = 0.1f;
    camera.far_plane = 2000.0f;
    camera.aspect = static_cast<float>(config.window_width) / config.window_height;
    
    // ========================================
    // Set up default lighting (sun + ambient)
    // ========================================
    LightingData lighting;
    
    // Sun light
    lighting.sun.direction = normalize(vec3{-0.5f, -0.7f, 0.3f});
    lighting.sun.color = {1.0f, 0.95f, 0.85f};
    lighting.sun.intensity = 1.5f;
    lighting.sun.cast_shadows = true;
    
    // Ambient (sky contribution)
    lighting.ambient_color = {0.2f, 0.25f, 0.3f};
    
    renderer.SetLighting(lighting);
    
    LOG_INFO("Engine initialized - empty scene");
    LOG_INFO("Use Node menu or right-click in Scene panel to add objects");
    LOG_INFO("Camera: Right-click drag to orbit, Middle-click to pan, Scroll to zoom, F to focus");
    
    // Run engine (editor mode - empty scene)
    engine.Run();
    
    // Cleanup
    engine.Shutdown();
    
    LOG_INFO("Engine exited normally");
    return 0;
}
