/*
 * Integration Tests — Physics World
 *
 * Verifies the JoltPhysics + ECS integration contract:
 *  - Initialize / Shutdown lifecycle
 *  - Dynamic body creation from ECS components
 *  - Gravity causes freefall (body position decreases in Y after several steps)
 *  - Accumulator cap (fix #34) prevents spiral-of-death on large dt
 *
 * Outside-In TDD: the observable behavior (body falls under gravity) was
 * specified here first; implementation wires up Jolt internally.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gameplay/ecs/ecs.h"
#include "physics/jolt/jolt_physics.h"
#include "physics/collision_shapes.h"

using namespace action;
using Catch::Approx;

// ── Fixture ───────────────────────────────────────────────────────────────────
struct PhysicsFixture {
    ECS ecs;
    JoltPhysics physics;

    PhysicsFixture() {
        ecs.Initialize();
        JoltPhysicsConfig cfg;
        cfg.num_threads = 1;                          // single-threaded for determinism
        cfg.temp_allocator_size = 2 * 1024 * 1024;   // 2 MB is enough for tests
        REQUIRE(physics.Initialize(&ecs, cfg));
    }

    ~PhysicsFixture() {
        physics.Shutdown();
        ecs.Shutdown();
    }

    // Helper: create a dynamic sphere entity at the given position
    Entity make_dynamic_sphere(const vec3& pos, float r = 0.5f) {
        Entity e = ecs.CreateEntity();

        TransformComponent tc;
        tc.position = pos;
        ecs.AddComponent<TransformComponent>(e, tc);

        ColliderComponent cc;
        cc.type   = ColliderType::Sphere;
        cc.radius = r;
        cc.layer  = CollisionLayer::Default;
        ecs.AddComponent<ColliderComponent>(e, cc);

        JPH::BodyID id = physics.CreateBody(e, /*is_dynamic=*/true);
        REQUIRE_FALSE(id.IsInvalid());
        return e;
    }
};

// ============================================================
// Lifecycle
// ============================================================

TEST_CASE("JoltPhysics - Initialize and Shutdown do not crash", "[physics][lifecycle]") {
    ECS ecs;
    ecs.Initialize();

    JoltPhysics jolt;
    JoltPhysicsConfig cfg;
    cfg.num_threads = 1;
    cfg.temp_allocator_size = 2 * 1024 * 1024;

    REQUIRE(jolt.Initialize(&ecs, cfg));
    REQUIRE_NOTHROW(jolt.Shutdown());
    REQUIRE_NOTHROW(ecs.Shutdown());
}

// ============================================================
// Gravity / freefall
// ============================================================

TEST_CASE("JoltPhysics - dynamic body falls under gravity over time", "[physics][gravity]") {
    PhysicsFixture f;
    const vec3 start{0.0f, 10.0f, 0.0f};
    Entity sphere = f.make_dynamic_sphere(start);

    // Run 60 steps (≈ 1 second at 60 Hz)
    for (int i = 0; i < 60; ++i) {
        f.physics.Update(1.0f / 60.0f);
        f.physics.SyncFromPhysics();
    }

    vec3 end = f.physics.GetPosition(sphere);
    // After 1 s under g = -25 m/s²: y ≈ 10 - 0.5*25*1 = -2.5
    // Use generous margin — we just need to confirm it fell
    REQUIRE(end.y < start.y - 5.0f);
}

// ============================================================
// Accumulator cap (fix #34)
// ============================================================

TEST_CASE("JoltPhysics - large delta time does not spiral (accumulator cap)", "[physics][accumulator]") {
    PhysicsFixture f;
    Entity sphere = f.make_dynamic_sphere({0, 10, 0});

    // Feed a huge dt (e.g., 5-second freeze frame)
    REQUIRE_NOTHROW(f.physics.Update(5.0f));

    // Physics must still produce a finite position
    vec3 pos = f.physics.GetPosition(sphere);
    REQUIRE(std::isfinite(pos.x));
    REQUIRE(std::isfinite(pos.y));
    REQUIRE(std::isfinite(pos.z));
}

// ============================================================
// Body position round-trip
// ============================================================

TEST_CASE("JoltPhysics - SetPosition / GetPosition round-trip", "[physics][body]") {
    PhysicsFixture f;
    Entity sphere = f.make_dynamic_sphere({0, 0, 0});

    const vec3 target{3.0f, 7.0f, -2.0f};
    f.physics.SetPosition(sphere, target);

    vec3 got = f.physics.GetPosition(sphere);
    REQUIRE(got.x == Approx(target.x).margin(0.01f));
    REQUIRE(got.y == Approx(target.y).margin(0.01f));
    REQUIRE(got.z == Approx(target.z).margin(0.01f));
}

// ============================================================
// Entity → BodyID round-trip
// ============================================================

TEST_CASE("JoltPhysics - GetBodyID and GetEntity are consistent", "[physics][body]") {
    PhysicsFixture f;
    Entity sphere = f.make_dynamic_sphere({1, 2, 3});

    JPH::BodyID id = f.physics.GetBodyID(sphere);
    REQUIRE_FALSE(id.IsInvalid());
    REQUIRE(f.physics.GetEntity(id) == sphere);
}
