#pragma once
#include "core/types.h"
#include "core/math/math.h"
#include "gameplay/ecs/ecs.h"

namespace action { class PhysicsWorld; }

namespace action {

enum class MountType : u8 {
    Horse      = 0,
    Wyvern     = 1,   // Flying
    Boat       = 2,
    Mechanical = 3
};

// Placed on mountable entities (horses, wyverns, …)
struct MountableComponent {
    MountType type            = MountType::Horse;
    float     max_speed       = 12.0f;
    float     acceleration    = 8.0f;
    float     turn_speed      = 2.5f;  // rad/s
    float     jump_force      = 7.0f;
    float     max_stamina     = 100.0f;
    float     current_stamina = 100.0f;
    float     stamina_drain   = 8.0f;   // per second while sprinting / airborne
    float     stamina_regen   = 12.0f;  // per second while resting

    Entity rider              = INVALID_ENTITY;
    bool   occupied           = false;
    bool   is_grounded        = true;

    vec3  current_velocity    = {0,0,0};
    vec3  rider_seat_offset   = {0, 1.8f, 0};  // local-space rider attachment

    // Desired movement this frame — set by game/input code before Update
    vec3  desired_input_dir   = {0,0,0};  // normalised world-space movement
    bool  jump_requested      = false;
    bool  sprint_requested    = false;

    bool IsFlyingMount()  const { return type == MountType::Wyvern; }
    float StaminaRatio()  const { return (max_stamina > 0.0f) ? current_stamina / max_stamina : 0.0f; }
};

// Placed on entities that can mount
struct RiderComponent {
    Entity mount              = INVALID_ENTITY;
    bool   is_mounted         = false;
    float  dismount_grace     = 0.5f;   // seconds of input-ignore after unmount
    float  dismount_timer     = 0.0f;
};

class MountSystem : public System {
public:
    MountSystem(ECS* ecs, PhysicsWorld* physics);

    // Returns true if the rider successfully mounted
    bool MountEntity(Entity rider, Entity mount);
    void DismountEntity(Entity rider);

    void Update(float dt) override;

private:
    void UpdateMountPhysics(Entity mount_entity, MountableComponent& mc, float dt);
    void SyncRiderTransform(Entity rider, Entity mount_entity, const MountableComponent& mc);

    ECS*          m_ecs     = nullptr;
    PhysicsWorld* m_physics = nullptr;
};

} // namespace action
