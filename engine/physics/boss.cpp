#include "boss.h"
#include "core/logging.h"
#include "core/math/math.h"

namespace action {

BossSystem::BossSystem(ECS* ecs, PhysicsWorld* physics)
    : m_ecs(ecs), m_physics(physics)
{
    LOG_INFO("BossSystem initialized");
}

bool BossSystem::HitBoss(Entity boss_entity, const std::string& hitbox_name, float damage) {
    BossComponent* boss = m_ecs->GetComponent<BossComponent>(boss_entity);
    if (!boss || !boss->IsAlive()) return false;

    for (const auto& hb : boss->hitboxes) {
        if (hb.bone_name != hitbox_name || !hb.active) continue;

        boss->TakeDamage(damage, hb.is_weak_point);

        if (hb.is_weak_point && boss->stagger_timer <= 0.0f) {
            boss->stagger_timer = 2.5f;
            boss->phase         = BossPhase::Staggered;
            boss->active_attack = -1;
            LOG_INFO("Boss '{}' staggered by weak-point hit on '{}'", boss->name, hitbox_name);
        }
        return true;
    }
    return false;
}

Entity BossSystem::FindNearestBoss(const vec3& world_pos, float max_range) {
    (void)world_pos; (void)max_range;
    Entity best = INVALID_ENTITY;
    m_ecs->ForEach<BossComponent>([&](Entity e, BossComponent& boss) {
        if (boss.IsAlive() && best == INVALID_ENTITY) best = e;
    });
    return best;
}

void BossSystem::UpdatePhase(BossComponent& boss) {
    if (!boss.IsAlive()) {
        if (boss.phase != BossPhase::Defeated) {
            boss.phase = BossPhase::Defeated;
            LOG_INFO("Boss '{}' defeated!", boss.name);
            if (boss.on_phase_change) boss.on_phase_change(BossPhase::Defeated);
            if (boss.on_defeated)     boss.on_defeated();
        }
        return;
    }

    if (boss.phase == BossPhase::Staggered) return; // Updated by stagger_timer

    float hp = boss.HealthRatio();
    BossPhase desired =
        (hp < boss.phase3_threshold) ? BossPhase::Phase3 :
        (hp < boss.phase2_threshold) ? BossPhase::Phase2 :
                                       BossPhase::Phase1;

    if (boss.phase == BossPhase::Idle || boss.phase == BossPhase::Approach) {
        boss.phase = BossPhase::Phase1;
        if (boss.on_phase_change) boss.on_phase_change(boss.phase);
    } else if (desired != boss.phase) {
        LOG_INFO("Boss '{}' enters phase {}", boss.name, static_cast<int>(desired));
        boss.phase = desired;
        if (boss.on_phase_change) boss.on_phase_change(desired);
    }
}

void BossSystem::UpdateAttacks(BossComponent& boss, float dt) {
    // Resize cooldown timers if game code appended attacks after construction
    if (boss.attack_timers.size() < boss.attacks.size())
        boss.attack_timers.resize(boss.attacks.size(), 0.0f);

    for (float& t : boss.attack_timers)
        if (t > 0.0f) t -= dt;

    if (boss.active_attack >= 0) {
        boss.attack_warmup -= dt;
        if (boss.attack_warmup <= 0.0f) {
            boss.attack_active -= dt;
            if (boss.attack_active <= 0.0f)
                boss.active_attack = -1;
        }
    }

    if (boss.stagger_timer > 0.0f) {
        boss.stagger_timer -= dt;
        if (boss.stagger_timer <= 0.0f) {
            boss.stagger_timer = 0.0f;
            if (boss.phase == BossPhase::Staggered)
                boss.phase = BossPhase::Phase1;
        }
    }
}

void BossSystem::UpdateAI(Entity /*entity*/, BossComponent& boss, float dt) {
    boss.aggro_timer -= dt;
    if (boss.aggro_timer > 0.0f) return;
    boss.aggro_timer = 0.5f; // Re-evaluate every 0.5 s

    if (m_player != INVALID_ENTITY) boss.target = m_player;

    // Pick an available attack
    if (boss.active_attack < 0) {
        for (u32 i = 0; i < static_cast<u32>(boss.attacks.size()); ++i) {
            const BossAttackDef& atk = boss.attacks[i];
            if (static_cast<u8>(boss.phase) < atk.min_phase) continue;
            if (boss.attack_timers[i] > 0.0f) continue;

            boss.active_attack     = static_cast<int>(i);
            boss.attack_warmup     = atk.warmup_time;
            boss.attack_active     = atk.active_time;
            boss.attack_timers[i]  = atk.cooldown;
            LOG_INFO("Boss '{}' initiates attack [{}] type={}", boss.name, i, static_cast<int>(atk.type));
            break;
        }
    }
}

void BossSystem::Update(float dt) {
    m_ecs->ForEach<BossComponent>([&](Entity e, BossComponent& boss) {
        if (boss.phase == BossPhase::Defeated) return;
        UpdatePhase(boss);
        if (boss.phase == BossPhase::Defeated) return;
        UpdateAttacks(boss, dt);
        UpdateAI(e, boss, dt);
    });
}

} // namespace action
