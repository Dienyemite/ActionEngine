#include "climbing.h"
#include "physics_world.h"
#include "collision_shapes.h"
#include "core/logging.h"
#include "core/math/math.h"
#include <algorithm>
#include <cmath>

namespace action {

ClimbingSystem::ClimbingSystem(ECS* ecs, PhysicsWorld* world)
    : m_ecs(ecs), m_world(world)
{
}

// ---------------------------------------------------------------------------
void ClimbingSystem::Update(float dt)
{
    m_ecs->ForEach<ClimbingComponent, TransformComponent>(
        [&](Entity entity, ClimbingComponent& climb, TransformComponent& transform)
    {
        // Tick cooldown timers
        if (climb.grab_cooldown > 0.0f)
            climb.grab_cooldown = std::max(0.0f, climb.grab_cooldown - dt);
        if (climb.latch_timer > 0.0f)
            climb.latch_timer   = std::max(0.0f, climb.latch_timer - dt);

        StaminaComponent* stamina = m_ecs->GetComponent<StaminaComponent>(entity);

        switch (climb.state)
        {
        case ClimbingState::Grounded:
        case ClimbingState::Airborne:
            ScanSurface(transform, climb);
            if (stamina) stamina->Regen(stamina->regen_rate * dt);
            break;

        case ClimbingState::GrabLatching:
            // Brief animation lock-out — latch_timer counts down to 0
            if (climb.latch_timer <= 0.0f) {
                climb.state = ClimbingState::Climbing;
                LOG_INFO("[Climbing] Entity {} entered Climbing state", entity);
            }
            break;

        case ClimbingState::Climbing:
        case ClimbingState::LedgeHang:
            UpdateAttachment(climb, transform);
            StepClimb(entity, dt, climb, transform, stamina);
            break;

        case ClimbingState::StaminaFall:
            // Slow regen while falling so stamina recovers while the player
            // is on the ground after a fall
            if (stamina) stamina->Regen(stamina->regen_rate * 0.2f * dt);
            break;
        }

        // Reset per-frame input — controller must set it again next frame
        climb.climb_input = {0.0f, 0.0f, 0.0f};
    });
}

// ---------------------------------------------------------------------------
bool ClimbingSystem::ProbeSurface(const vec3& from, const vec3& dir,
                                   float reach, GripPoint& out_grip) const
{
    RaycastHit hit = m_world->Raycast(from, dir, reach,
                                       CollisionLayer::All, INVALID_ENTITY);
    if (!hit) return false;

    // Verify the hit entity has (or lacks) a ClimbableComponent
    if (hit.entity != INVALID_ENTITY) {
        const auto* climbable = m_ecs->GetComponent<ClimbableComponent>(hit.entity);
        if (climbable && !climbable->is_climbable) return false;
    }

    out_grip.position = hit.point;
    out_grip.normal   = hit.normal;
    out_grip.entity   = hit.entity;
    out_grip.valid    = true;
    return true;
}

// ---------------------------------------------------------------------------
void ClimbingSystem::ScanSurface(const TransformComponent& transform,
                                  ClimbingComponent& climb) const
{
    // Cast from chest height toward entity forward to detect climbable surfaces
    const vec3 chest_pos = transform.position + vec3{0.0f, 1.2f, 0.0f};
    const vec3 forward   = transform.rotation * vec3{0.0f, 0.0f, 1.0f};

    GripPoint grip;
    if (ProbeSurface(chest_pos, forward, climb.grip_reach + 0.4f, grip)) {
        climb.grips[0]       = grip;
        climb.surface_normal = grip.normal;
    }
}

// ---------------------------------------------------------------------------
void ClimbingSystem::StepClimb(Entity entity, float dt,
                                ClimbingComponent& climb,
                                TransformComponent& transform,
                                StaminaComponent* stamina)
{
    // Determine effective friction from whichever grip hit a climbable entity
    float friction = 1.0f;
    for (const auto& grip : climb.grips) {
        if (!grip.valid || grip.entity == INVALID_ENTITY) continue;
        const auto* c = m_ecs->GetComponent<ClimbableComponent>(grip.entity);
        if (c) friction = std::min(friction, c->friction);
    }

    // Drain stamina scaled by surface friction
    if (stamina) {
        stamina->Drain(stamina->drain_rate * friction * dt);
        if (stamina->IsEmpty()) {
            TransitionToStaminaFall(entity, climb);
            return;
        }
    }

    // Project climb_input onto the current surface plane and move
    const vec3& input = climb.climb_input;
    if (input.length_sq() > 0.0001f) {
        const float along = input.dot(climb.surface_normal);
        vec3 projected = input - climb.surface_normal * along;
        if (projected.length_sq() > 0.0001f) {
            transform.position = transform.position +
                                  projected.normalized() * climb.climb_speed * dt;
        }
        // Refresh surface scan after the entity moved
        ScanSurface(transform, climb);
    }
}

// ---------------------------------------------------------------------------
void ClimbingSystem::UpdateAttachment(ClimbingComponent& climb,
                                       TransformComponent& transform)
{
    BoneAttachment& att = climb.attachment;
    if (!att.active) return;

    if (!m_ecs->IsAlive(att.host_entity)) {
        att.active = false;
        return;
    }

    // --- Bone-level callback (set by game code for animated bosses) ---
    if (att.bone_world_pos_fn) {
        transform.position = att.bone_world_pos_fn() + att.local_offset;
        return;
    }

    // --- Entity-root attachment (translates + rotates with the host) ---
    const TransformComponent* host = m_ecs->GetComponent<TransformComponent>(att.host_entity);
    if (!host) {
        att.active = false;
        return;
    }

    // Rotate local_offset by current host rotation, then offset from host position
    const vec3 world_offset = host->rotation * att.local_offset;
    transform.position      = host->position + world_offset;
}

// ---------------------------------------------------------------------------
void ClimbingSystem::SetClimbInput(Entity entity, const vec3& world_dir)
{
    auto* climb = m_ecs->GetComponent<ClimbingComponent>(entity);
    if (climb) climb->climb_input = world_dir;
}

// ---------------------------------------------------------------------------
void ClimbingSystem::RequestGrab(Entity entity, const vec3& preferred_dir)
{
    auto* climb     = m_ecs->GetComponent<ClimbingComponent>(entity);
    auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
    if (!climb || !transform) return;
    if (!climb->CanGrab())  return;

    // Disallow grab while stamina is depleted
    const auto* stamina = m_ecs->GetComponent<StaminaComponent>(entity);
    if (stamina && stamina->is_depleted) return;

    const vec3 dir = (preferred_dir.length_sq() > 0.0001f)
                         ? preferred_dir.normalized()
                         : (transform->rotation * vec3{0.0f, 0.0f, 1.0f});

    const vec3 chest_pos = transform->position + vec3{0.0f, 1.2f, 0.0f};

    GripPoint grip;
    if (!ProbeSurface(chest_pos, dir, climb->grip_reach * 1.6f, grip)) return;

    // Activate the primary grip
    climb->grips[0]       = grip;
    climb->surface_normal = grip.normal;
    climb->state          = ClimbingState::GrabLatching;
    climb->latch_timer    = climb->latch_duration;

    // If the surface belongs to a dynamic entity (boss), parent to it
    if (grip.entity != INVALID_ENTITY) {
        const auto* climbable = m_ecs->GetComponent<ClimbableComponent>(grip.entity);
        if (climbable && climbable->is_dynamic) {
            const auto* host_t = m_ecs->GetComponent<TransformComponent>(grip.entity);
            if (host_t) {
                BoneAttachment& att = climb->attachment;
                att.host_entity         = grip.entity;
                // Store climber offset in host local frame (inverse-rotated)
                att.local_offset        = host_t->rotation.conjugate() *
                                          (transform->position - host_t->position);
                att.bone_world_pos_fn   = nullptr;  // game code may override this
                att.active              = true;
            }
        }
    }

    LOG_DEBUG("[Climbing] Entity {} grabbed surface at ({:.2f},{:.2f},{:.2f})",
              entity, grip.position.x, grip.position.y, grip.position.z);
}

// ---------------------------------------------------------------------------
void ClimbingSystem::RequestRelease(Entity entity)
{
    auto* climb = m_ecs->GetComponent<ClimbingComponent>(entity);
    if (!climb || !climb->IsOnSurface()) return;

    TransitionToAirborne(entity, *climb);
    LOG_DEBUG("[Climbing] Entity {} released grip", entity);
}

// ---------------------------------------------------------------------------
void ClimbingSystem::RequestLedgeVault(Entity entity)
{
    auto* climb     = m_ecs->GetComponent<ClimbingComponent>(entity);
    auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
    if (!climb || !transform) return;
    if (climb->state != ClimbingState::LedgeHang) return;

    // Push up over the ledge edge
    transform->position = transform->position + vec3{0.0f, 1.8f, 0.0f};
    TransitionToAirborne(entity, *climb);
    LOG_INFO("[Climbing] Entity {} vaulted ledge", entity);
}

// ---------------------------------------------------------------------------
void ClimbingSystem::TransitionToAirborne(Entity entity, ClimbingComponent& climb)
{
    climb.state             = ClimbingState::Airborne;
    climb.attachment.active = false;
    climb.grab_cooldown     = climb.grab_cooldown_dur;
    for (auto& g : climb.grips) g.valid = false;
}

// ---------------------------------------------------------------------------
void ClimbingSystem::TransitionToStaminaFall(Entity entity, ClimbingComponent& climb)
{
    climb.state             = ClimbingState::StaminaFall;
    climb.attachment.active = false;
    climb.grab_cooldown     = 1.5f;  // prevent immediate re-grab after stamina fall
    for (auto& g : climb.grips) g.valid = false;
    LOG_WARN("[Climbing] Entity {} fell from surface — stamina depleted", entity);
}

} // namespace action
