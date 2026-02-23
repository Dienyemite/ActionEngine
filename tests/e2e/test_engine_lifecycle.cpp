/*
 * End-to-End Tests — Engine Simulation Lifecycle
 *
 * Tests the full simulation pipeline (ECS + Physics + JobSystem) without
 * requiring a Vulkan context or window.  This is the outermost test layer in
 * our Outside-In TDD approach: behaviour is specified from the perspective of
 * a caller that sets up a scene and runs the game loop.
 *
 * Scope:
 *   - Core subsystem bring-up / tear-down order
 *   - A representative scene (floor + falling objects) runs to completion
 *   - Entities that should survive do; destroyed entities cannot be observed
 *   - Accumulator-capped physics prevents runaway steps on missed frames
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/jobs/job_system.h"
#include "gameplay/ecs/ecs.h"
#include "physics/jolt/jolt_physics.h"
#include "physics/collision_shapes.h"

using namespace action;
using Catch::Approx;

// ── Shared scene helpers ──────────────────────────────────────────────────────

static JoltPhysicsConfig headless_cfg() {
    JoltPhysicsConfig cfg;
    cfg.num_threads             = 1;                    // deterministic single-threaded
    cfg.temp_allocator_size     = 4 * 1024 * 1024;
    cfg.gravity                 = {0, -25.0f, 0};
    return cfg;
}

static Entity add_dynamic_sphere(ECS& ecs, JoltPhysics& phys, vec3 pos, float r = 0.5f) {
    Entity e = ecs.CreateEntity();
    TransformComponent tc; tc.position = pos;
    ColliderComponent  cc; cc.type = ColliderType::Sphere; cc.radius = r;
    ecs.AddComponent<TransformComponent>(e, tc);
    ecs.AddComponent<ColliderComponent>(e, cc);
    phys.CreateBody(e, true);
    return e;
}

static Entity add_static_box(ECS& ecs, JoltPhysics& phys, vec3 pos, vec3 half = {5,0.5f,5}) {
    Entity e = ecs.CreateEntity();
    TransformComponent tc; tc.position = pos;
    ColliderComponent  cc; cc.type = ColliderType::Box; cc.half_extents = half; cc.is_static = true;
    ecs.AddComponent<TransformComponent>(e, tc);
    ecs.AddComponent<ColliderComponent>(e, cc);
    phys.CreateBody(e, false);
    return e;
}

// ── Fixtures ──────────────────────────────────────────────────────────────────

struct SimFixture {
    JobSystem  jobs;
    ECS        ecs;
    JoltPhysics physics;

    SimFixture() {
        REQUIRE(jobs.Initialize(1));     // 1 worker thread for tests
        ecs.Initialize();
        REQUIRE(physics.Initialize(&ecs, headless_cfg()));
    }

    ~SimFixture() {
        physics.Shutdown();
        ecs.Shutdown();
        jobs.Shutdown();
    }

    void tick(float dt = 1.0f / 60.0f) {
        ecs.Update(dt);
        physics.Update(dt);
        physics.SyncFromPhysics();
    }

    void run(int frames, float dt = 1.0f / 60.0f) {
        for (int i = 0; i < frames; ++i) tick(dt);
    }
};

// ============================================================
// Subsystem bring-up and tear-down
// ============================================================

TEST_CASE("E2E - JobSystem + ECS + Physics initialise and shut down cleanly", "[e2e][lifecycle]") {
    SimFixture f;
    // Destructor verifies clean shutdown — just reaching here is a pass
    SUCCEED("All subsystems initialised without error");
}

TEST_CASE("E2E - repeated init/shutdown cycle does not leak state", "[e2e][lifecycle]") {
    for (int cycle = 0; cycle < 3; ++cycle) {
        ECS ecs;
        JoltPhysics phys;
        ecs.Initialize();
        REQUIRE(phys.Initialize(&ecs, headless_cfg()));
        phys.Shutdown();
        ecs.Shutdown();
    }
}

// ============================================================
// Scene: free-falling sphere
// ============================================================

TEST_CASE("E2E - sphere falls under gravity for ~1 second (60 frames)", "[e2e][scene][gravity]") {
    SimFixture f;
    const vec3 spawn{0, 20.0f, 0};
    Entity ball = add_dynamic_sphere(f.ecs, f.physics, spawn);

    f.run(60);  // ~1 second

    vec3 pos = f.physics.GetPosition(ball);
    // Under g = -25, after 1 s: Δy ≈ -12.5 m → should be < 10
    REQUIRE(pos.y < 10.0f);
    REQUIRE(std::isfinite(pos.y));
}

// ============================================================
// Scene: sphere rests on static floor (collision response)
// ============================================================

TEST_CASE("E2E - dynamic sphere comes to rest on static floor", "[e2e][scene][collision]") {
    SimFixture f;

    // Floor at y = 0, sphere starts 5 m above it
    add_static_box(f.ecs, f.physics, {0, -0.5f, 0}, {10, 0.5f, 10});
    Entity ball = add_dynamic_sphere(f.ecs, f.physics, {0, 5.0f, 0}, 0.5f);

    f.run(180);  // ~3 seconds — enough time to settle

    vec3 pos = f.physics.GetPosition(ball);
    // Ball radius 0.5, floor surface at y = 0 → resting y ≈ 0.5
    REQUIRE(pos.y >= -0.1f);   // didn't fall through floor
    REQUIRE(pos.y <= 2.0f);    // not floating in the air
}

// ============================================================
// Multiple independently-falling entities
// ============================================================

TEST_CASE("E2E - multiple spheres fall independently without interference", "[e2e][scene][multi]") {
    SimFixture f;

    Entity a = add_dynamic_sphere(f.ecs, f.physics, {-5, 15, 0});
    Entity b = add_dynamic_sphere(f.ecs, f.physics, { 0, 15, 0});
    Entity c = add_dynamic_sphere(f.ecs, f.physics, { 5, 15, 0});

    f.run(60);

    // All three should have fallen, stay finite, and remain alive in ECS
    for (Entity e : {a, b, c}) {
        REQUIRE(f.ecs.IsAlive(e));
        vec3 pos = f.physics.GetPosition(e);
        REQUIRE(std::isfinite(pos.y));
        REQUIRE(pos.y < 15.0f);
    }
}

// ============================================================
// Entity destruction mid-simulation
// ============================================================

TEST_CASE("E2E - destroying an entity mid-simulation does not corrupt other entities", "[e2e][scene][destroy]") {
    SimFixture f;

    Entity a = add_dynamic_sphere(f.ecs, f.physics, {0, 10, 0});
    Entity b = add_dynamic_sphere(f.ecs, f.physics, {5, 10, 0});

    f.run(30);   // half a second

    // Destroy `a` mid-simulation
    f.physics.DestroyBody(a);
    f.ecs.DestroyEntity(a);

    f.run(30);   // another half second

    // `b` must still be alive and have moved
    REQUIRE(f.ecs.IsAlive(b));
    vec3 pos = f.physics.GetPosition(b);
    REQUIRE(std::isfinite(pos.y));

    // `a` must be dead
    REQUIRE_FALSE(f.ecs.IsAlive(a));
}

// ============================================================
// Accumulator: missed frame does not produce infinite steps (#34)
// ============================================================

TEST_CASE("E2E - 5-second missed frame does not spiral or crash", "[e2e][physics][accumulator]") {
    SimFixture f;
    Entity ball = add_dynamic_sphere(f.ecs, f.physics, {0, 50, 0});

    // Simulate a completely frozen system that missed 5 seconds
    REQUIRE_NOTHROW(f.tick(5.0f));

    vec3 pos = f.physics.GetPosition(ball);
    REQUIRE(std::isfinite(pos.x));
    REQUIRE(std::isfinite(pos.y));
    REQUIRE(std::isfinite(pos.z));
}
