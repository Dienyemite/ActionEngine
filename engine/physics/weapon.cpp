#include "weapon.h"
#include "core/logging.h"
#include "core/math/math.h"

namespace action {

// ---------------------------------------------------------------------------
// WeaponSystem
// ---------------------------------------------------------------------------

WeaponSystem::WeaponSystem(ECS* ecs, PhysicsWorld* physics)
    : m_ecs(ecs), m_physics(physics)
{
    LOG_INFO("WeaponSystem initialized");
}

void WeaponSystem::QueueAttack(Entity entity) {
    WeaponComponent* wc = m_ecs->GetComponent<WeaponComponent>(entity);
    if (!wc) return;
    if (wc->cooldown_timer > 0.0f) return;
    wc->attack_queued = true;
}

Entity WeaponSystem::FireProjectile(Entity owner_entity, const vec3& origin, const vec3& dir) {
    WeaponComponent* wc = m_ecs->GetComponent<WeaponComponent>(owner_entity);
    if (!wc) return INVALID_ENTITY;
    if (wc->config.current_ammo == 0) {
        LOG_INFO("Weapon '{}' out of ammo", wc->name);
        return INVALID_ENTITY;
    }
    if (wc->config.current_ammo != UINT32_MAX)
        --wc->config.current_ammo;

    Entity proj = m_ecs->CreateEntity();
    ProjectileComponent& pc = m_ecs->AddComponent<ProjectileComponent>(proj);
    pc.velocity        = dir * wc->config.projectile_speed;
    pc.damage          = wc->config.damage;
    pc.lifetime        = wc->config.projectile_life;
    pc.age             = 0.0f;
    pc.owner           = owner_entity;
    pc.piercing        = wc->config.piercing;
    pc.active          = true;
    pc.gravity_enabled = (wc->config.type == WeaponType::Arrow ||
                          wc->config.type == WeaponType::Thrown);

    // Projectile spawned at `origin`; a PositionComponent would store world position.
    // For now the data is recorded; the renderer / ProjectileSystem handles movement.
    LOG_INFO("Projectile spawned from entity {} at ({},{},{})", owner_entity,
             origin.x, origin.y, origin.z);
    return proj;
}

void WeaponSystem::ProcessMelee(Entity entity, WeaponComponent& wc) {
    // Broad-phase melee: OverlapSphere within attack_range
    // Full implementation depends on having a world-position component.
    // Calls PhysicsWorld::OverlapSphere once a position source is available.
    wc.attack_active = 0.15f; // short hit-window
    wc.is_attacking  = true;
    LOG_INFO("Entity {} melee attack '{}' (range={:.2f})", entity, wc.name, wc.config.attack_range);
}

void WeaponSystem::Update(float dt) {
    m_ecs->ForEach<WeaponComponent>([&](Entity e, WeaponComponent& wc) {
        if (wc.cooldown_timer > 0.0f)
            wc.cooldown_timer -= dt;

        if (wc.attack_active > 0.0f) {
            wc.attack_active -= dt;
            if (wc.attack_active <= 0.0f) {
                wc.attack_active = 0.0f;
                wc.is_attacking  = false;
            }
        }

        if (wc.attack_queued && wc.cooldown_timer <= 0.0f) {
            wc.attack_queued = false;
            if (wc.config.type == WeaponType::Melee) {
                ProcessMelee(e, wc);
            }
            // Ranged fire is explicit via FireProjectile()
            wc.cooldown_timer = 1.0f / std::max(0.01f, wc.config.attack_speed);
        }
    });
}

// ---------------------------------------------------------------------------
// ProjectileSystem
// ---------------------------------------------------------------------------

ProjectileSystem::ProjectileSystem(ECS* ecs, PhysicsWorld* physics)
    : m_ecs(ecs), m_physics(physics)
{
    LOG_INFO("ProjectileSystem initialized");
}

void ProjectileSystem::Update(float dt) {
    static const float GRAVITY = 9.81f;

    std::vector<Entity> to_destroy;

    m_ecs->ForEach<ProjectileComponent>([&](Entity e, ProjectileComponent& pc) {
        if (!pc.active) { to_destroy.push_back(e); return; }

        pc.age += dt;
        if (pc.age >= pc.lifetime) { to_destroy.push_back(e); return; }

        if (pc.gravity_enabled)
            pc.velocity.y -= GRAVITY * pc.gravity_scale * dt;

        // Movement is stored in velocity; actual position update requires a PositionComponent.
        // Full impl: pos += pc.velocity * dt; SphereCast for hit detection.
    });

    for (Entity e : to_destroy)
        m_ecs->DestroyEntity(e);
}

} // namespace action
