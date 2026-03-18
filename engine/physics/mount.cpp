#include "mount.h"
#include "physics_world.h"
#include "core/logging.h"
#include "core/math/math.h"

namespace action {

MountSystem::MountSystem(ECS* ecs, PhysicsWorld* physics)
    : m_ecs(ecs), m_physics(physics)
{
    LOG_INFO("MountSystem initialized");
}

bool MountSystem::MountEntity(Entity rider, Entity mount) {
    MountableComponent* mc = m_ecs->GetComponent<MountableComponent>(mount);
    RiderComponent*     rc = m_ecs->GetComponent<RiderComponent>(rider);
    if (!mc || !rc) return false;
    if (mc->occupied)         return false;
    if (rc->dismount_timer > 0.0f) return false;

    mc->occupied = true;
    mc->rider    = rider;
    rc->is_mounted = true;
    rc->mount      = mount;
    LOG_INFO("Entity {} mounted entity {}", rider, mount);
    return true;
}

void MountSystem::DismountEntity(Entity rider) {
    RiderComponent* rc = m_ecs->GetComponent<RiderComponent>(rider);
    if (!rc || !rc->is_mounted) return;

    MountableComponent* mc = m_ecs->GetComponent<MountableComponent>(rc->mount);
    if (mc) {
        mc->occupied = false;
        mc->rider    = INVALID_ENTITY;
    }
    rc->is_mounted      = false;
    rc->mount           = INVALID_ENTITY;
    rc->dismount_timer  = rc->dismount_grace;
    LOG_INFO("Entity {} dismounted", rider);
}

void MountSystem::UpdateMountPhysics(Entity /*mount_entity*/, MountableComponent& mc, float dt) {
    if (mc.IsFlyingMount()) {
        if (!mc.is_grounded) {
            mc.current_stamina -= mc.stamina_drain * dt;
            if (mc.current_stamina < 0.0f) mc.current_stamina = 0.0f;
        } else {
            mc.current_stamina = std::min(mc.max_stamina,
                mc.current_stamina + mc.stamina_regen * dt);
        }
    } else if (mc.sprint_requested && mc.occupied) {
        mc.current_stamina -= mc.stamina_drain * dt;
        if (mc.current_stamina < 0.0f) mc.current_stamina = 0.0f;
    } else {
        mc.current_stamina = std::min(mc.max_stamina,
            mc.current_stamina + mc.stamina_regen * dt);
    }

    // Simple velocity integration towards desired direction
    vec3 target_vel = mc.desired_input_dir * mc.max_speed;
    float blend = std::min(1.0f, mc.acceleration * dt);
    mc.current_velocity.x = Lerp(mc.current_velocity.x, target_vel.x, blend);
    mc.current_velocity.z = Lerp(mc.current_velocity.z, target_vel.z, blend);

    if (!mc.is_grounded && !mc.IsFlyingMount()) {
        // Basic gravity for non-flying mounts
        mc.current_velocity.y -= 9.81f * dt;
    } else {
        mc.current_velocity.y = 0.0f;
    }

    // Handle jump
    if (mc.jump_requested && mc.is_grounded) {
        mc.current_velocity.y = mc.jump_force;
        mc.is_grounded = false;
    }
    mc.jump_requested = false;
    mc.sprint_requested = false;
}

void MountSystem::SyncRiderTransform(Entity rider, Entity mount_entity, const MountableComponent& mc) {
    auto* mount_tc = m_ecs->GetComponent<TransformComponent>(mount_entity);
    auto* rider_tc = m_ecs->GetComponent<TransformComponent>(rider);
    if (!mount_tc || !rider_tc) return;

    // Rotate the seat offset by the mount's orientation (quaternion × vec3)
    vec3 world_offset = mount_tc->rotation * mc.rider_seat_offset;

    rider_tc->position = {
        mount_tc->position.x + world_offset.x,
        mount_tc->position.y + world_offset.y,
        mount_tc->position.z + world_offset.z
    };
    rider_tc->rotation = mount_tc->rotation;
}

void MountSystem::Update(float dt) {
    // Tick dismount grace timers
    m_ecs->ForEach<RiderComponent>([&](Entity /*e*/, RiderComponent& rc) {
        if (rc.dismount_timer > 0.0f)
            rc.dismount_timer -= dt;
    });

    // Update mounted physics
    m_ecs->ForEach<MountableComponent>([&](Entity e, MountableComponent& mc) {
        UpdateMountPhysics(e, mc, dt);

        // Apply velocity to world position via TransformComponent
        if (auto* tc = m_ecs->GetComponent<TransformComponent>(e)) {
            tc->position.x += mc.current_velocity.x * dt;
            tc->position.y += mc.current_velocity.y * dt;
            tc->position.z += mc.current_velocity.z * dt;

            // Simple ground clamp — real impl uses PhysicsWorld raycast
            if (tc->position.y < 0.0f) {
                tc->position.y = 0.0f;
                mc.current_velocity.y = 0.0f;
                mc.is_grounded = true;
            }
        }

        if (mc.occupied && mc.rider != INVALID_ENTITY)
            SyncRiderTransform(mc.rider, e, mc);
    });
}

} // namespace action
