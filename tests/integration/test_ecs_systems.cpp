/*
 * Integration Tests — ECS Systems
 *
 * Verifies that the ECS ForEach iteration, System registration, and multi-
 * component queries work correctly when multiple subsystems interact.
 *
 * Outside-In TDD: the interface of a "movement system" was defined here first;
 * the ForEach/smallest-pool improvements followed.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>

#include "gameplay/ecs/ecs.h"

using namespace action;
using Catch::Approx;

// ── Test components ───────────────────────────────────────────────────────────
struct Position { float x = 0, y = 0, z = 0; };
struct Velocity { float vx = 0, vy = 0, vz = 0; };
struct Mass     { float kg = 1.0f; };

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::unique_ptr<ECS> make_ecs() {
    auto ecs = std::make_unique<ECS>();
    ecs->Initialize();
    return ecs;
}

// ============================================================
// ForEach — single-component iteration
// ============================================================

TEST_CASE("ECS ForEach - visits all entities with the queried component", "[ecs][foreach]") {
    auto ecs = make_ecs();

    Entity a = ecs->CreateEntity();
    Entity b = ecs->CreateEntity();
    Entity c = ecs->CreateEntity();  // no Position — must NOT be visited

    ecs->AddComponent<Position>(a, {1, 0, 0});
    ecs->AddComponent<Position>(b, {2, 0, 0});
    ecs->AddComponent<Velocity>(c, {0, 1, 0});  // different component, c skipped

    int count = 0;
    ecs->ForEach<Position>([&](Entity, Position& p) {
        REQUIRE((p.x == 1.0f || p.x == 2.0f));  // only a and b
        ++count;
    });

    REQUIRE(count == 2);
}

// ============================================================
// ForEach — multi-component intersection (movement system contract)
// ============================================================

TEST_CASE("ECS ForEach - only visits entities with ALL required components", "[ecs][foreach][multi]") {
    auto ecs = make_ecs();

    Entity moving    = ecs->CreateEntity();  // has Position + Velocity
    Entity stationary = ecs->CreateEntity(); // only Position
    Entity massless  = ecs->CreateEntity();  // only Velocity

    ecs->AddComponent<Position>(moving,    {0, 0, 0});
    ecs->AddComponent<Velocity>(moving,    {1, 0, 0});
    ecs->AddComponent<Position>(stationary, {5, 0, 0});
    ecs->AddComponent<Velocity>(massless,  {0, 2, 0});

    int visited = 0;
    ecs->ForEach<Position, Velocity>([&](Entity e, Position&, Velocity&) {
        REQUIRE(e == moving);  // only `moving` has both
        ++visited;
    });

    REQUIRE(visited == 1);
}

// ============================================================
// Simple in-test movement system using ForEach
// ============================================================

TEST_CASE("ECS - movement system integrates velocity into position", "[ecs][system]") {
    auto ecs = make_ecs();
    const float dt = 0.016f;  // ~60 FPS frame

    Entity e = ecs->CreateEntity();
    ecs->AddComponent<Position>(e, {0.0f, 0.0f, 0.0f});
    ecs->AddComponent<Velocity>(e, {10.0f, 0.0f, 0.0f});

    // Simulate movement via ForEach (what a MovementSystem.Update() would call)
    ecs->ForEach<Position, Velocity>([dt](Entity, Position& p, Velocity& v) {
        p.x += v.vx * dt;
        p.y += v.vy * dt;
        p.z += v.vz * dt;
    });

    Position* pos = ecs->GetComponent<Position>(e);
    REQUIRE(pos != nullptr);
    REQUIRE(pos->x == Approx(10.0f * dt).epsilon(1e-5f));
}

// ============================================================
// ForEach — empty pool short-circuits gracefully
// ============================================================

TEST_CASE("ECS ForEach - no entities with component does not crash", "[ecs][foreach][edge]") {
    auto ecs = make_ecs();
    // No entities at all — ForEach must silently return
    int count = 0;
    REQUIRE_NOTHROW(ecs->ForEach<Position>([&](Entity, Position&) { ++count; }));
    REQUIRE(count == 0);
}

// ============================================================
// System registration
// ============================================================

class CountingSystem : public System {
public:
    int update_count = 0;
    void Update(float) override { ++update_count; }
};

TEST_CASE("ECS - registered system is called on ECS::Update", "[ecs][system][registration]") {
    auto ecs = make_ecs();
    CountingSystem* sys = ecs->AddSystem<CountingSystem>();

    ecs->Update(0.016f);
    ecs->Update(0.016f);

    REQUIRE(sys->update_count == 2);
}

TEST_CASE("ECS - disabled system is not called on Update", "[ecs][system][registration]") {
    auto ecs = make_ecs();
    CountingSystem* sys = ecs->AddSystem<CountingSystem>();
    sys->SetEnabled(false);

    ecs->Update(0.016f);

    REQUIRE(sys->update_count == 0);
}
