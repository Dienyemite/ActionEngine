#include "engine/engine.h"
#include "core/logging.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include "physics/collision_shapes.h"
#include "physics/character_controller.h"
#include "scripting/builtin_scripts.h"
#include "editor/editor.h"

int main() {
    using namespace action;
    
    LOG_INFO("ActionEngine Starting...");
    
    // Register all builtin scripts (PlayerController, etc.)
    RegisterBuiltinScripts();
    
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
    
    // ========================================
    // Skip test scene when using project system
    // Set to false to start with an empty editor
    // ========================================
    constexpr bool CREATE_TEST_SCENE = false;
    
    ECS& ecs = engine.GetECS();
    ScriptSystem& scripts = engine.GetScripts();
    PhysicsWorld& physics = engine.GetPhysics();
    AssetManager& assets = engine.GetAssets();
    
    // Create primitive meshes (always needed for editor)
    MeshHandle cube_mesh = assets.CreateCubeMesh(1.0f);
    MeshHandle ground_mesh = assets.CreateCubeMesh(1.0f);
    MaterialHandle default_material{0};
    
    if (CREATE_TEST_SCENE) {
    // ========================================
    // Test Scene: Physics + Scripting Integration
    // ========================================
    
    // --- Create Player Cube ---
    Entity player = ecs.CreateEntity();
    ecs.SetPlayerEntity(player);
    
    // Transform (start above ground to test falling/landing)
    auto& player_transform = ecs.AddComponent<TransformComponent>(player);
    player_transform.position = {0, 3, 0};
    player_transform.scale = {1, 1, 1};
    
    // Tag as player
    auto& player_tag = ecs.AddComponent<TagComponent>(player);
    player_tag.name = "Player";
    player_tag.tags = Tags::Player | Tags::Dynamic;
    
    // Render component (visible cube)
    auto& player_render = ecs.AddComponent<RenderComponent>(player);
    player_render.mesh = cube_mesh;
    player_render.material = default_material;
    player_render.visible = true;
    
    // Collider for physics queries
    auto& player_collider = ecs.AddComponent<ColliderComponent>(player);
    player_collider.type = ColliderType::Capsule;
    player_collider.radius = 0.4f;
    player_collider.height = 1.8f;
    player_collider.layer = CollisionLayer::Player;
    player_collider.mask = CollisionLayer::Environment | CollisionLayer::Enemy;
    
    // Character controller for kinematic movement
    auto& player_cc = ecs.AddComponent<CharacterControllerComponent>(player);
    player_cc.config.height = 1.8f;
    player_cc.config.radius = 0.4f;
    player_cc.config.step_height = 0.35f;
    player_cc.config.slope_limit = 50.0f;
    player_cc.coyote_time = 0.12f;
    player_cc.jump_buffer_duration = 0.15f;
    
    // Add PlayerController script
    auto* player_script = scripts.AddScript<PlayerController>(player);
    player_script->move_speed = 8.0f;
    player_script->sprint_multiplier = 1.6f;
    player_script->jump_force = 10.0f;
    
    // Add rigidbody for Jolt collision (kinematic - moved by CharacterController)
    auto& player_rb = ecs.AddComponent<RigidbodyComponent>(player);
    player_rb.is_kinematic = true;  // Player is controlled by CharacterController, not physics
    player_rb.mass = 80.0f;         // 80 kg player for realistic pushes
    
    LOG_INFO("Created player entity with PlayerController script");
    
    // --- Create Ground Plane (large box collider) ---
    Entity ground = ecs.CreateEntity();
    
    auto& ground_transform = ecs.AddComponent<TransformComponent>(ground);
    ground_transform.position = {0, -0.5f, 0};  // Top surface at y=0
    ground_transform.scale = {100, 1, 100};     // 100x100 ground
    
    auto& ground_tag = ecs.AddComponent<TagComponent>(ground);
    ground_tag.name = "Ground";
    ground_tag.tags = Tags::Static;
    
    // Render component (visible ground plane)
    auto& ground_render = ecs.AddComponent<RenderComponent>(ground);
    ground_render.mesh = ground_mesh;
    ground_render.material = default_material;
    ground_render.visible = true;
    
    auto& ground_collider = ecs.AddComponent<ColliderComponent>(ground);
    ground_collider.type = ColliderType::Box;
    ground_collider.half_extents = {50, 0.5f, 50};
    ground_collider.layer = CollisionLayer::Environment;
    ground_collider.mask = CollisionLayer::Player | CollisionLayer::Enemy | CollisionLayer::Projectile;
    ground_collider.is_static = true;
    
    // Register ground with physics world
    physics.AddCollider(ground);
    
    LOG_INFO("Created ground plane");
    
    // --- Create some obstacle boxes for testing ---
    for (int i = 0; i < 5; ++i) {
        Entity box = ecs.CreateEntity();
        
        auto& box_transform = ecs.AddComponent<TransformComponent>(box);
        box_transform.position = {static_cast<float>(i * 4 - 8), 0.5f, 8};
        box_transform.scale = {1, 1, 1};
        
        auto& box_tag = ecs.AddComponent<TagComponent>(box);
        box_tag.name = "Obstacle_" + std::to_string(i);
        box_tag.tags = Tags::Static | Tags::Prop;
        
        // Render component
        auto& box_render = ecs.AddComponent<RenderComponent>(box);
        box_render.mesh = cube_mesh;
        box_render.material = default_material;
        box_render.visible = true;
        
        auto& box_collider = ecs.AddComponent<ColliderComponent>(box);
        box_collider.type = ColliderType::Box;
        box_collider.half_extents = {0.5f, 0.5f, 0.5f};
        box_collider.layer = CollisionLayer::Environment;
        box_collider.is_static = true;
        
        physics.AddCollider(box);
    }
    
    LOG_INFO("Created 5 obstacle boxes");
    
    // --- Create Dynamic Rigidbody Boxes (for Jolt physics testing) ---
    JoltPhysics& jolt = engine.GetJoltPhysics();
    
    for (int i = 0; i < 10; ++i) {
        Entity dyn_box = ecs.CreateEntity();
        
        // Stacked and offset for interesting physics
        float x = (i % 5) * 1.2f - 2.4f;
        float y = 5.0f + (i / 5) * 1.2f;  // Start above ground
        float z = 5.0f + (i % 3) * 0.3f;  // Slight offset for tumbling
        
        auto& dyn_transform = ecs.AddComponent<TransformComponent>(dyn_box);
        dyn_transform.position = {x, y, z};
        dyn_transform.scale = {1, 1, 1};
        
        auto& dyn_tag = ecs.AddComponent<TagComponent>(dyn_box);
        dyn_tag.name = "DynamicBox_" + std::to_string(i);
        dyn_tag.tags = Tags::Dynamic | Tags::Prop;
        
        // Render component
        auto& dyn_render = ecs.AddComponent<RenderComponent>(dyn_box);
        dyn_render.mesh = cube_mesh;
        dyn_render.material = default_material;
        dyn_render.visible = true;
        
        auto& dyn_collider = ecs.AddComponent<ColliderComponent>(dyn_box);
        dyn_collider.type = ColliderType::Box;
        dyn_collider.half_extents = {0.5f, 0.5f, 0.5f};
        dyn_collider.layer = CollisionLayer::Default;
        dyn_collider.is_static = false;
        
        // Rigidbody component for physics simulation
        auto& dyn_rb = ecs.AddComponent<RigidbodyComponent>(dyn_box);
        dyn_rb.mass = 10.0f;              // 10 kg boxes
        dyn_rb.friction = 0.6f;           // Moderate friction
        dyn_rb.restitution = 0.2f;        // Low bounce (weighty)
        dyn_rb.linear_damping = 0.05f;    // Slight air resistance
        dyn_rb.angular_damping = 0.1f;    // Dampens spinning
        dyn_rb.gravity_factor = 1.0f;     // Full gravity
        
        // Create Jolt body for this entity
        jolt.CreateBody(dyn_box, true);  // true = dynamic
    }
    
    LOG_INFO("Created 10 dynamic rigidbody boxes (Jolt Physics)");
    
    // --- Create a static ground body in Jolt too ---
    jolt.CreateBody(ground, false);  // false = static
    
    // --- Create kinematic player body in Jolt (for pushing dynamic objects) ---
    jolt.CreateBody(player, true);   // true = dynamic (but is_kinematic in RigidbodyComponent)
    
    // --- Create a step/platform for testing step-up ---
    Entity step = ecs.CreateEntity();
    
    auto& step_transform = ecs.AddComponent<TransformComponent>(step);
    step_transform.position = {5, 0.15f, 0};  // Low step
    step_transform.scale = {2, 0.3f, 2};
    
    auto& step_tag = ecs.AddComponent<TagComponent>(step);
    step_tag.name = "Step";
    step_tag.tags = Tags::Static;
    
    // Render component
    auto& step_render = ecs.AddComponent<RenderComponent>(step);
    step_render.mesh = cube_mesh;
    step_render.material = default_material;
    step_render.visible = true;
    
    auto& step_collider = ecs.AddComponent<ColliderComponent>(step);
    step_collider.type = ColliderType::Box;
    step_collider.half_extents = {1, 0.15f, 1};
    step_collider.layer = CollisionLayer::Environment;
    step_collider.is_static = true;
    
physics.AddCollider(step);
    jolt.CreateBody(step, false);  // Static in Jolt
    
    LOG_INFO("Created step platform for step-up testing");
    
    // Position camera to follow player
    camera.position = {0, 8, -12};
    camera.forward = normalize(vec3{0, -0.4f, 1});
    
    LOG_INFO("==============================================");
    LOG_INFO("PHYSICS + SCRIPTING TEST SCENE LOADED");
    LOG_INFO("Controls:");
    LOG_INFO("  WASD    - Move");
    LOG_INFO("  Shift   - Sprint");
    LOG_INFO("  Space   - Jump");
    LOG_INFO("  Escape  - Exit play mode");
    LOG_INFO("  F5      - Toggle play mode");
    LOG_INFO("==============================================");

    // Start in play mode for testing
    engine.GetEditor().SetPlayMode(true);
    
    } // End CREATE_TEST_SCENE block
    
    // When no test scene, start in editor mode
    if (!CREATE_TEST_SCENE) {
        LOG_INFO("==============================================");
        LOG_INFO("ActionEngine Editor Mode");
        LOG_INFO("Use File > New Project to get started");
        LOG_INFO("==============================================");
        engine.GetEditor().SetPlayMode(false);
    }

    // ========================================
    // DEMO SCENE: Boss + Mount + NavMesh + Audio + Cinematic
    // ========================================
    constexpr bool CREATE_DEMO_SCENE = true;
    if (CREATE_DEMO_SCENE) {
        AudioSystem&    audio    = engine.GetAudioSystem();
        NavMesh&        navmesh  = engine.GetNavMesh();
        TerrainSystem&  terrain  = engine.GetTerrain();
        BossSystem&     bossSys  = engine.GetBossSystem();
        NavMeshSystem&  navSys   = engine.GetNavMeshSystem();
        CinematicSystem& cinSys  = engine.GetCinematicSystem();

        // --- 1. Audio: load clips and start music ---
        audio.LoadClip("music_main",  "assets/audio/main_theme.ogg",    180.0f, true,  SoundCategory::Music);
        audio.LoadClip("sfx_sword",   "assets/audio/sword_swing.wav",   0.5f,  false, SoundCategory::SFX);
        audio.LoadClip("sfx_gallop",  "assets/audio/horse_gallop.wav",  2.0f,  true,  SoundCategory::SFX);
        audio.LoadClip("sfx_roar",    "assets/audio/boss_roar.wav",     3.0f,  false, SoundCategory::SFX);
        audio.PlayMusic("music_main", 2.0f);

        // --- 2. Build navmesh from terrain height ---
        navmesh.Build(-200.0f, -200.0f, 200.0f, 200.0f, 2.0f, 40.0f,
            [&terrain](float x, float z) { return terrain.GetHeightAt(x, z); });

        // --- 3. Create player entity (needed before boss targeting) ---
        Entity player = ecs.CreateEntity();
        ecs.SetPlayerEntity(player);
        {
            auto& tc = ecs.AddComponent<TransformComponent>(player);
            tc.position = {0.0f, 0.0f, -10.0f};
            auto& tag = ecs.AddComponent<TagComponent>(player);
            tag.name = "Player";
            tag.tags = Tags::Player | Tags::Dynamic;
            auto& rc = ecs.AddComponent<RenderComponent>(player);
            rc.mesh = cube_mesh; rc.material = default_material; rc.visible = true;
            auto& cc = ecs.AddComponent<ColliderComponent>(player);
            cc.type = ColliderType::Capsule; cc.radius = 0.4f; cc.height = 1.8f;
            cc.layer = CollisionLayer::Player;
            cc.mask  = CollisionLayer::Environment | CollisionLayer::Enemy;
            // Give player a sword
            auto& wc = ecs.AddComponent<WeaponComponent>(player);
            wc.name = "Iron Sword";
            wc.owner = player;
            wc.config.type = WeaponType::Melee;
            wc.config.damage = 35.0f;
            wc.config.attack_range = 2.5f;
            wc.config.attack_speed = 1.5f;
        }
        bossSys.SetPlayerEntity(player);

        // --- 4. Boss entity (Giant Troll) ---
        Entity boss_ent = ecs.CreateEntity();
        {
            auto& tc = ecs.AddComponent<TransformComponent>(boss_ent);
            tc.position = {30.0f, 0.0f, 50.0f};
            auto& tag = ecs.AddComponent<TagComponent>(boss_ent);
            tag.name = "GiantTroll";
            tag.tags = Tags::Enemy;
            auto& rc = ecs.AddComponent<RenderComponent>(boss_ent);
            rc.mesh = cube_mesh; rc.material = default_material; rc.visible = true;
            auto& bc = ecs.AddComponent<BossComponent>(boss_ent);
            bc.name = "Giant Troll";
            bc.max_health = bc.current_health = 5000.0f;
            bc.aggro_range = 80.0f;
            bc.phase2_threshold = 0.65f;
            bc.phase3_threshold = 0.30f;
            bc.on_phase_change = [](BossPhase p) {
                LOG_INFO("Boss entered phase {}", static_cast<int>(p));
            };
            bc.on_defeated = [&audio]() {
                LOG_INFO("Boss defeated!");
                audio.PlayOneShot("sfx_roar", {30.0f, 0.0f, 50.0f}, 0.8f);
            };
            // Hitboxes
            bc.hitboxes.push_back({"head",  {{0.0f, 3.0f, 0.0f}, 0.8f}, 1.0f, true,  true});
            bc.hitboxes.push_back({"chest", {{0.0f, 1.5f, 0.0f}, 1.2f}, 1.0f, false, true});
            bc.hitboxes.push_back({"leg_l", {{-0.7f, 0.5f, 0.0f}, 0.5f}, 0.8f, false, true});
            bc.hitboxes.push_back({"leg_r", {{ 0.7f, 0.5f, 0.0f}, 0.5f}, 0.8f, false, true});
            bc.attack_timers.resize(4, 0.0f);
            // Attacks
            BossAttackDef stomp;
            stomp.type = BossAttackType::Stomp; stomp.damage = 80.0f;
            stomp.range = 4.0f; stomp.cooldown = 5.0f; stomp.min_phase = 0;
            bc.attacks.push_back(stomp);
            BossAttackDef sweep;
            sweep.type = BossAttackType::Sweep; sweep.damage = 60.0f;
            sweep.range = 6.0f; sweep.cooldown = 8.0f; sweep.min_phase = 0;
            bc.attacks.push_back(sweep);
            BossAttackDef charge;
            charge.type = BossAttackType::Charge; charge.damage = 120.0f;
            charge.range = 15.0f; charge.cooldown = 12.0f; charge.min_phase = 1;
            bc.attacks.push_back(charge);
            BossAttackDef roar;
            roar.type = BossAttackType::Roar; roar.damage = 0.0f;
            roar.range = 20.0f; roar.cooldown = 20.0f; roar.min_phase = 2;
            bc.attacks.push_back(roar);
            // Collider for damage registration
            auto& col = ecs.AddComponent<ColliderComponent>(boss_ent);
            col.type = ColliderType::Capsule; col.radius = 1.5f; col.height = 4.0f;
            col.layer = CollisionLayer::Enemy;
            col.mask  = CollisionLayer::Player | CollisionLayer::Projectile;
            physics.AddCollider(boss_ent);
        }
        LOG_INFO("Demo: Boss 'Giant Troll' created at (30,0,50)");

        // --- 5. Mount (Horse) ---
        Entity horse = ecs.CreateEntity();
        {
            auto& tc = ecs.AddComponent<TransformComponent>(horse);
            tc.position = {-20.0f, 0.0f, 10.0f};
            auto& tag = ecs.AddComponent<TagComponent>(horse);
            tag.name = "Horse";
            auto& rc = ecs.AddComponent<RenderComponent>(horse);
            rc.mesh = cube_mesh; rc.material = default_material; rc.visible = true;
            auto& mc = ecs.AddComponent<MountableComponent>(horse);
            mc.type = MountType::Horse;
            mc.max_speed = 12.0f;
            mc.acceleration = 8.0f;
            mc.rider_seat_offset = {0.0f, 1.5f, 0.0f};
            auto& sc = ecs.AddComponent<SoundComponent>(horse);
            sc.clip_name = "sfx_gallop"; sc.spatial = true; sc.loop = true; sc.play_on_start = false;
        }
        LOG_INFO("Demo: Horse mount created at (-20,0,10)");

        // --- 6. Patrol enemy with NavAgent ---
        Entity patrol = ecs.CreateEntity();
        {
            auto& tc = ecs.AddComponent<TransformComponent>(patrol);
            tc.position = {10.0f, 0.0f, 20.0f};
            auto& tag = ecs.AddComponent<TagComponent>(patrol);
            tag.name = "PatrolGuard";
            tag.tags = Tags::Enemy;
            auto& rc = ecs.AddComponent<RenderComponent>(patrol);
            rc.mesh = cube_mesh; rc.material = default_material; rc.visible = true;
            auto& col = ecs.AddComponent<ColliderComponent>(patrol);
            col.type = ColliderType::Capsule; col.radius = 0.4f; col.height = 1.8f;
            col.layer = CollisionLayer::Enemy;
            col.mask  = CollisionLayer::Player | CollisionLayer::Environment;
            auto& nav = ecs.AddComponent<NavAgentComponent>(patrol);
            nav.speed = 4.0f;
            nav.arrival_radius = 1.0f;
            nav.path_recalc_period = 2.0f;
            navSys.SetDestination(patrol, {-10.0f, 0.0f, -20.0f});
        }
        LOG_INFO("Demo: Patrol guard created, navigating to (-10,0,-20)");

        // --- 7. Cinematic intro sequence ---
        Entity cinematic_ent = ecs.CreateEntity();
        {
            auto& tag = ecs.AddComponent<TagComponent>(cinematic_ent);
            tag.name = "IntroSequence";
            auto& cc = ecs.AddComponent<CinematicComponent>(cinematic_ent);
            cc.clip.name         = "intro";
            cc.clip.duration     = 8.0f;
            cc.clip.skip_allowed = true;
            // Camera sweep from far to close-up on boss
            cc.clip.camera_track.push_back({0.0f, {0.0f,  15.0f, -30.0f}, {0.0f,  0.0f, 50.0f}, 40.0f, 1.0f});
            cc.clip.camera_track.push_back({4.0f, {20.0f,  8.0f,  10.0f}, {30.0f, 2.0f, 50.0f}, 60.0f, 2.0f});
            cc.clip.camera_track.push_back({8.0f, {0.0f,   5.0f, -15.0f}, {0.0f,  0.0f,  0.0f}, 75.0f, 1.0f});
            // Dialogue
            cc.clip.dialogue.push_back({1.0f, 4.0f, "The Troll awakens from its slumber...", "Narrator"});
            cc.clip.dialogue.push_back({5.0f, 8.0f, "Prepare yourself, warrior.", "Narrator"});
            cc.clip.on_end = [&engine]() {
                engine.GetEditor().SetPlayMode(true);
                LOG_INFO("Cinematic done — entering play mode");
            };
            cinSys.PlayCinematic(cinematic_ent);
        }
        LOG_INFO("Demo: Intro cinematic playing (8s)");

        // Aim camera at the action during cinematic
        camera.position = {0.0f, 15.0f, -30.0f};
        camera.forward = normalize(vec3{0.0f, -0.3f, 1.0f});

        LOG_INFO("==============================================");
        LOG_INFO("DEMO SCENE LOADED");
        LOG_INFO("  Boss: Giant Troll at (30,0,50)");
        LOG_INFO("  Mount: Horse at (-20,0,10)");
        LOG_INFO("  NavAgent patrol: (10,0,20) -> (-10,0,-20)");
        LOG_INFO("  Cinematic intro: 8 seconds");
        LOG_INFO("==============================================");
    }

    // Run engine
    engine.Run();
    
    // Cleanup
    engine.Shutdown();
    
    LOG_INFO("Engine exited normally");
    return 0;
}
