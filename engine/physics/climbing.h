#pragma once

/*
 * Climbing & Grip System
 *
 * Provides SotC-style surface climbing:
 *   - StaminaComponent   : stamina pool that drains while climbing
 *   - ClimbableComponent : tag any entity/boss as a climbable surface
 *   - ClimbingComponent  : added to the player; tracks state and grip points
 *   - ClimbingSystem     : ECS System that drives all climbing logic
 *
 * Architecture notes:
 *   - Lives in EnginePhysics so it can use PhysicsWorld raycasts.
 *   - Bone-level attachment (climbing a boss's arm) is supported via an
 *     optional std::function<vec3()> callback set by game code; this avoids
 *     a circular dependency with EngineAnimation.
 */

#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"
#include <array>
#include <functional>

namespace action {

class PhysicsWorld;

// =====================================================================
// StaminaComponent
// Tracks a stamina / grip-endurance pool.
// =====================================================================
struct StaminaComponent {
    float max_stamina     = 100.0f;
    float current_stamina = 100.0f;
    float drain_rate      = 12.0f;  // units per second while actively climbing
    float regen_rate      = 25.0f;  // units per second while resting / grounded

    // is_depleted stays true until current_stamina reaches the recovery threshold
    bool  is_depleted     = false;

    float NormalizedStamina() const { return current_stamina / max_stamina; }
    bool  IsEmpty()           const { return current_stamina <= 0.0f; }
    bool  IsFull()            const { return current_stamina >= max_stamina; }

    void Drain(float amount) {
        current_stamina -= amount;
        if (current_stamina < 0.0f) current_stamina = 0.0f;
        if (current_stamina <= 0.0f) is_depleted = true;
    }

    void Regen(float amount) {
        current_stamina += amount;
        if (current_stamina > max_stamina) current_stamina = max_stamina;
        // Require recovering to 5 % before allowing re-grab
        if (is_depleted && current_stamina >= max_stamina * 0.05f)
            is_depleted = false;
    }
};

// =====================================================================
// ClimbableComponent
// Tag any wall, boss, or ledge entity as climbable.
// =====================================================================
struct ClimbableComponent {
    bool  is_climbable = true;
    // Multiplier on stamina drain: soft fur ≈ 0.6, normal rock = 1.0, wet stone ≈ 1.5
    float friction     = 1.0f;
    // true → entity moves each frame (boss body part, animated platform)
    bool  is_dynamic   = false;
};

// =====================================================================
// GripPoint – a single contact between the climber and a surface
// =====================================================================
struct GripPoint {
    vec3   position{0.0f, 0.0f, 0.0f};
    vec3   normal{0.0f, 1.0f, 0.0f};
    Entity entity = INVALID_ENTITY;
    bool   valid  = false;
};

// =====================================================================
// BoneAttachment
// Keeps the climber parented to a moving entity (e.g. a boss).
// For bone-level precision, set bone_world_pos_fn from game code after
// RequestGrab() returns; ClimbingSystem will call it each frame.
// =====================================================================
struct BoneAttachment {
    Entity host_entity = INVALID_ENTITY;

    // Climber offset stored in the host's LOCAL rotated coordinate frame.
    // Each frame:  world_pos = host.position + host.rotation * local_offset
    vec3   local_offset{0.0f, 0.0f, 0.0f};

    // Optional: set by game code for bone-level attachment.
    // Should return the current world-space position of the target bone.
    std::function<vec3()> bone_world_pos_fn;

    bool active = false;
};

// =====================================================================
// ClimbingState
// =====================================================================
enum class ClimbingState : u8 {
    Grounded     = 0,  // standing on floor
    Airborne     = 1,  // airborne (not climbing)
    GrabLatching = 2,  // brief animation lock-out while reaching for surface
    Climbing     = 3,  // actively climbing
    LedgeHang    = 4,  // hanging from ledge edge, waiting for vault input
    StaminaFall  = 5,  // fell because stamina ran out
};

// =====================================================================
// ClimbingComponent
// Attach to any entity that should be able to climb (player, AI, etc.).
// =====================================================================
struct ClimbingComponent {
    ClimbingState state = ClimbingState::Grounded;

    // Tuning
    float grip_reach  = 0.9f;   // max raycast length when scanning for surfaces
    float climb_speed = 2.5f;   // metres per second along surface

    // Grip points: [0]=LeftHand  [1]=RightHand  [2]=LeftFoot  [3]=RightFoot
    std::array<GripPoint, 4> grips{};

    // Bone / entity attachment (filled by ClimbingSystem on successful grab)
    BoneAttachment attachment;

    // Per-frame movement intent; set by controller, consumed by ClimbingSystem
    vec3 climb_input{0.0f, 0.0f, 0.0f};

    // Current surface normal we are hanging against
    vec3 surface_normal{0.0f, 0.0f, 1.0f};

    // Timers
    float grab_cooldown     = 0.0f;
    float grab_cooldown_dur = 0.35f;
    float latch_timer       = 0.0f;   // counts down during GrabLatching
    float latch_duration    = 0.22f;

    // ---- Helpers ----
    bool CanGrab()     const { return grab_cooldown <= 0.0f && latch_timer <= 0.0f && !IsOnSurface(); }
    bool IsOnSurface() const { return state == ClimbingState::Climbing || state == ClimbingState::LedgeHang; }
    bool IsActive()    const { return state != ClimbingState::Grounded && state != ClimbingState::Airborne; }
};

// =====================================================================
// ClimbingSystem
// Register this with ECS after PhysicsWorld is initialised.
// =====================================================================
class ClimbingSystem : public System {
public:
    ClimbingSystem(ECS* ecs, PhysicsWorld* world);

    void Update(float dt) override;

    // ---- Game-side API ------------------------------------------------

    // Supply the desired movement direction for this frame (world space).
    // Called each frame by the PlayerController before ECS::Update().
    void SetClimbInput(Entity entity, const vec3& world_dir);

    // Attempt to grab the nearest climbable surface.
    // preferred_dir = hint direction (zero → use entity forward).
    void RequestGrab(Entity entity, const vec3& preferred_dir = {0.0f, 0.0f, 0.0f});

    // Release the current grip, enter Airborne.
    void RequestRelease(Entity entity);

    // While in LedgeHang state, vault upward onto the ledge.
    void RequestLedgeVault(Entity entity);

private:
    // Shoots a ray; validates ClimbableComponent; fills out_grip on hit.
    bool ProbeSurface(const vec3& from, const vec3& dir,
                      float reach, GripPoint& out_grip) const;

    // Scan for climbable surfaces in front of the entity.
    void ScanSurface(const TransformComponent& transform,
                     ClimbingComponent& climb) const;

    // Apply stamina drain, move entity along surface.
    void StepClimb(Entity entity, float dt,
                   ClimbingComponent& climb,
                   TransformComponent& transform,
                   StaminaComponent* stamina);

    // Update player world position to follow an attached moving entity.
    void UpdateAttachment(ClimbingComponent& climb, TransformComponent& transform);

    void TransitionToAirborne(Entity entity, ClimbingComponent& climb);
    void TransitionToStaminaFall(Entity entity, ClimbingComponent& climb);

    ECS*          m_ecs   = nullptr;
    PhysicsWorld* m_world = nullptr;
};

} // namespace action
