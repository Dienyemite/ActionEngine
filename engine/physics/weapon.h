#pragma once
#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include <string>
#include <climits>

namespace action { class PhysicsWorld; }

namespace action {

enum class WeaponType : u8 {
    Melee    = 0,
    Arrow    = 1,
    Magic    = 2,
    Thrown   = 3
};

struct WeaponConfig {
    WeaponType type             = WeaponType::Melee;
    float      damage           = 25.0f;
    float      attack_range     = 2.5f;    // melee radius (m)
    float      attack_speed     = 1.5f;    // attacks per second
    float      projectile_speed = 25.0f;   // m/s (ranged only)
    float      projectile_life  = 8.0f;    // seconds
    bool       piercing         = false;
    u32        max_ammo         = UINT32_MAX;   // UINT32_MAX = infinite
    u32        current_ammo     = UINT32_MAX;
};

struct WeaponComponent {
    WeaponConfig config;
    std::string  name;
    float        cooldown_timer  = 0.0f;
    bool         is_attacking    = false;
    bool         attack_queued   = false;
    float        attack_active   = 0.0f;   // remaining window (s)
    Entity       owner           = INVALID_ENTITY;
};

// One projectile in flight
struct ProjectileComponent {
    vec3   velocity       = {0,0,0};
    float  damage         = 10.0f;
    float  lifetime       = 8.0f;
    float  age            = 0.0f;
    Entity owner          = INVALID_ENTITY;
    bool   piercing       = false;
    bool   active         = true;
    bool   gravity_enabled = true;
    float  gravity_scale  = 1.0f;
    float  radius         = 0.15f;
};

class WeaponSystem : public System {
public:
    WeaponSystem(ECS* ecs, PhysicsWorld* physics);

    // Mark an attack as requested — executed on next Update
    void   QueueAttack(Entity weapon_entity);
    // Spawn a projectile travelling in `dir` (normalised)
    Entity FireProjectile(Entity owner_entity, const vec3& origin, const vec3& dir);

    void Update(float dt) override;

private:
    void ProcessMelee(Entity entity, WeaponComponent& wc);

    ECS*          m_ecs     = nullptr;
    PhysicsWorld* m_physics = nullptr;
};

class ProjectileSystem : public System {
public:
    ProjectileSystem(ECS* ecs, PhysicsWorld* physics);
    void Update(float dt) override;

private:
    ECS*          m_ecs     = nullptr;
    PhysicsWorld* m_physics = nullptr;
};

} // namespace action
