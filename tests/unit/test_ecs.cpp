/*
 * Unit Tests — ECS
 *
 * Outside-In TDD: tests define the expected entity/component lifecycle before
 * (or alongside) implementation changes. Covers fix #39 (bitmask O(K) destroy).
 */
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "gameplay/ecs/ecs.h"

using namespace action;

// ── Minimal test components ──────────────────────────────────────────────────
struct Position { float x = 0, y = 0, z = 0; };
struct Velocity { float x = 0, y = 0, z = 0; };
struct Health   { int current = 100, max = 100; };

// ── Helpers ───────────────────────────────────────────────────────────────────
// ECS has a user-declared destructor which suppresses the implicit move
// constructor; returning by unique_ptr avoids any copy/move of the object.
static std::unique_ptr<ECS> make_ecs() {
    auto ecs = std::make_unique<ECS>();
    ecs->Initialize();
    return ecs;
}

// ============================================================
// Entity creation / destruction
// ============================================================

TEST_CASE("ECS - newly created entity is alive", "[ecs][entity]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();
    REQUIRE(e != INVALID_ENTITY);
    REQUIRE(ecs->IsAlive(e));
}

TEST_CASE("ECS - destroyed entity is no longer alive", "[ecs][entity]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();
    ecs->DestroyEntity(e);
    REQUIRE_FALSE(ecs->IsAlive(e));
}

TEST_CASE("ECS - recycled index carries incremented generation", "[ecs][entity]") {
    auto ecs = make_ecs();
    Entity first = ecs->CreateEntity();
    u32 first_gen = EntityGeneration(first);

    ecs->DestroyEntity(first);
    Entity reused = ecs->CreateEntity();  // should reuse same index

    // The old handle is stale — alive check uses generation
    REQUIRE_FALSE(ecs->IsAlive(first));
    REQUIRE(ecs->IsAlive(reused));
    REQUIRE(EntityIndex(reused)      == EntityIndex(first));
    REQUIRE(EntityGeneration(reused) >  first_gen);
}

TEST_CASE("ECS - INVALID_ENTITY is never alive", "[ecs][entity]") {
    auto ecs = make_ecs();
    REQUIRE_FALSE(ecs->IsAlive(INVALID_ENTITY));
}

// ============================================================
// Component add / get / remove
// ============================================================

TEST_CASE("ECS - component added and retrieved", "[ecs][component]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();

    Position& pos = ecs->AddComponent<Position>(e, {1.0f, 2.0f, 3.0f});
    REQUIRE(pos.x == 1.0f);

    Position* got = ecs->GetComponent<Position>(e);
    REQUIRE(got != nullptr);
    REQUIRE(got->x == 1.0f);
}

TEST_CASE("ECS - HasComponent returns correct truth value", "[ecs][component]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();

    REQUIRE_FALSE(ecs->HasComponent<Health>(e));
    ecs->AddComponent<Health>(e);
    REQUIRE(ecs->HasComponent<Health>(e));
}

TEST_CASE("ECS - removed component is no longer accessible", "[ecs][component]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();

    ecs->AddComponent<Velocity>(e, {1.0f, 0.0f, 0.0f});
    REQUIRE(ecs->HasComponent<Velocity>(e));

    ecs->RemoveComponent<Velocity>(e);
    REQUIRE_FALSE(ecs->HasComponent<Velocity>(e));
    REQUIRE(ecs->GetComponent<Velocity>(e) == nullptr);
}

TEST_CASE("ECS - EmplaceComponent constructs in place", "[ecs][component]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();

    Health& h = ecs->EmplaceComponent<Health>(e, Health{50, 100});
    REQUIRE(h.current == 50);
    REQUIRE(h.max     == 100);
}

// ============================================================
// Bitmask optimization (#39): DestroyEntity only visits active pools
// ============================================================

TEST_CASE("ECS - destroy clears all components from destroyed entity", "[ecs][component][mask]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();

    ecs->AddComponent<Position>(e, {1, 2, 3});
    ecs->AddComponent<Velocity>(e, {0, 1, 0});
    ecs->AddComponent<Health>(e);

    ecs->DestroyEntity(e);

    // After destroy the entity is gone — components must not linger in pools
    // (Verified via a fresh entity recycled to the same index.)
    Entity e2 = ecs->CreateEntity();  // reuses index
    REQUIRE_FALSE(ecs->HasComponent<Position>(e2));
    REQUIRE_FALSE(ecs->HasComponent<Velocity>(e2));
    REQUIRE_FALSE(ecs->HasComponent<Health>(e2));
}

TEST_CASE("ECS - destroying entity without components does not crash", "[ecs][component][mask]") {
    auto ecs = make_ecs();
    Entity e = ecs->CreateEntity();
    REQUIRE_NOTHROW(ecs->DestroyEntity(e));
}

// ============================================================
// Multiple independent entities
// ============================================================

TEST_CASE("ECS - components on one entity do not affect another", "[ecs][isolation]") {
    auto ecs = make_ecs();
    Entity a = ecs->CreateEntity();
    Entity b = ecs->CreateEntity();

    ecs->AddComponent<Position>(a, {10, 0, 0});
    ecs->AddComponent<Position>(b, {99, 0, 0});

    REQUIRE(ecs->GetComponent<Position>(a)->x == 10.0f);
    REQUIRE(ecs->GetComponent<Position>(b)->x == 99.0f);

    ecs->RemoveComponent<Position>(a);
    REQUIRE_FALSE(ecs->HasComponent<Position>(a));
    REQUIRE(ecs->HasComponent<Position>(b));  // b unaffected
}
