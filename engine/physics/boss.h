#pragma once

/*
 * Feature 3.1 — Complex Boss Entities
 *
 * BossComponent  : full boss state (HP, phases, attacks, hitboxes).
 * BossSystem     : updates phase transitions, attack AI, stagger timers.
 *
 * Lives in EnginePhysics so it can issue PhysicsWorld raycasts / overlap
 * queries.  The boss integrates with the ClimbingSystem via the
 * is_being_climbed / climber_count fields that ClimbingSystem writes.
 */

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include "physics_world.h"
#include <vector>
#include <string>
#include <functional>

namespace action {

enum class BossPhase : u8 { Idle, Approach, Phase1, Phase2, Phase3, Staggered, Defeated };
enum class BossAttackType : u8 { Stomp, Sweep, Charge, Lunge, Roar };

struct BossHitbox {
    std::string bone_name;
    Sphere      local_bounds    = {{0, 0, 0}, 1.0f};
    float       damage_mult     = 1.0f;
    bool        is_weak_point   = false;  // Weak spots: ×2.5 damage + stagger
    bool        active          = true;
};

struct BossAttackDef {
    BossAttackType type         = BossAttackType::Stomp;
    float          damage       = 80.0f;
    float          range        = 6.0f;         // World units
    float          cooldown     = 4.0f;         // Seconds between uses
    float          warmup_time  = 0.5f;         // Wind-up duration
    float          active_time  = 0.3f;         // Active hitbox window
    u8             min_phase    = 0;            // Available from this phase index
};

struct BossComponent {
    std::string                name;
    float                      max_health         = 5000.0f;
    float                      current_health     = 5000.0f;
    BossPhase                  phase              = BossPhase::Idle;
    float                      phase2_threshold   = 0.65f;  // HP ratio
    float                      phase3_threshold   = 0.30f;  // HP ratio

    std::vector<BossHitbox>    hitboxes;
    std::vector<BossAttackDef> attacks;
    std::vector<float>         attack_timers;     // parallel to attacks[]

    // Active attack state
    int    active_attack   = -1;
    float  attack_warmup   = 0.0f;
    float  attack_active   = 0.0f;
    float  stagger_timer   = 0.0f;
    float  aggro_timer     = 0.0f;

    // Targeting
    Entity target          = INVALID_ENTITY;
    float  aggro_range     = 80.0f;

    // Climbing integration (written by ClimbingSystem)
    bool   is_being_climbed = false;
    u32    climber_count    = 0;

    // Callbacks — set by game code
    std::function<void(BossPhase)>  on_phase_change;
    std::function<void(float,bool)> on_damage;      // (amount, was_weak_point)
    std::function<void()>           on_defeated;

    float HealthRatio() const { return max_health > 0.0f ? current_health / max_health : 0.0f; }
    bool  IsAlive()     const { return current_health > 0.0f; }

    void TakeDamage(float amount, bool weak_point = false) {
        float mult   = weak_point ? 2.5f : 1.0f;
        float actual = amount * mult;
        current_health -= actual;
        if (current_health < 0.0f) current_health = 0.0f;
        if (on_damage) on_damage(actual, weak_point);
    }
};

class BossSystem : public System {
public:
    BossSystem(ECS* ecs, PhysicsWorld* physics);
    void Update(float dt) override;

    void SetPlayerEntity(Entity player) { m_player = player; }

    // Apply damage to a named hitbox; returns true if valid hit
    bool HitBoss(Entity boss_entity, const std::string& hitbox_name, float damage);

    // Find first alive boss (distance-based in full impl; name-scan here)
    Entity FindNearestBoss(const vec3& world_pos, float max_range = 10000.0f);

private:
    ECS*          m_ecs    = nullptr;
    PhysicsWorld* m_physics = nullptr;
    Entity        m_player  = INVALID_ENTITY;

    void UpdatePhase(BossComponent& boss);
    void UpdateAI(Entity entity, BossComponent& boss, float dt);
    void UpdateAttacks(BossComponent& boss, float dt);
};

} // namespace action
