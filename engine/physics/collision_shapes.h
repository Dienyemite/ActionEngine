#pragma once

#include "core/types.h"
#include "core/math/math.h"

namespace action {

/*
 * Collision Shapes for Physics System
 * 
 * Designed for fast-paced action games:
 * - Capsule: Best for characters (handles steps, slopes naturally)
 * - Box: Environmental geometry
 * - Sphere: Projectiles, triggers
 */

enum class ColliderType : u8 {
    None = 0,
    Sphere,
    Box,
    Capsule,
    Mesh        // For static geometry only
};

// Collision layers for filtering
enum class CollisionLayer : u32 {
    None        = 0,
    Default     = 1 << 0,
    Player      = 1 << 1,
    Enemy       = 1 << 2,
    Environment = 1 << 3,
    Trigger     = 1 << 4,
    Projectile  = 1 << 5,
    Interactable = 1 << 6,
    All         = 0xFFFFFFFF
};

inline CollisionLayer operator|(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline CollisionLayer operator&(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline bool HasLayer(CollisionLayer mask, CollisionLayer layer) {
    return (static_cast<u32>(mask) & static_cast<u32>(layer)) != 0;
}

/*
 * Capsule - Two hemispheres connected by a cylinder
 * Perfect for character controllers
 */
struct Capsule {
    vec3 center{0, 0, 0};   // Center of capsule
    float radius = 0.5f;     // Radius of hemispheres and cylinder
    float height = 2.0f;     // Total height (including hemispheres)
    
    // Get the line segment endpoints (centers of hemispheres)
    vec3 GetTop() const { 
        float half_line = (height - 2.0f * radius) * 0.5f;
        return center + vec3{0, half_line, 0}; 
    }
    
    vec3 GetBottom() const { 
        float half_line = (height - 2.0f * radius) * 0.5f;
        return center - vec3{0, half_line, 0}; 
    }
    
    // Get AABB bounds
    AABB GetBounds() const {
        float half_height = height * 0.5f;
        return AABB{
            center - vec3{radius, half_height, radius},
            center + vec3{radius, half_height, radius}
        };
    }
};

/*
 * ColliderComponent - Attached to entities for collision
 */
struct ColliderComponent {
    ColliderType type = ColliderType::None;
    CollisionLayer layer = CollisionLayer::Default;
    CollisionLayer mask = CollisionLayer::All;  // What layers this collides with
    
    // Shape data (union-like usage based on type)
    vec3 offset{0, 0, 0};       // Local offset from entity position
    
    // Sphere/Capsule
    float radius = 0.5f;
    
    // Capsule
    float height = 2.0f;
    
    // Box
    vec3 half_extents{0.5f, 0.5f, 0.5f};
    
    // Flags
    bool is_trigger = false;    // Triggers don't block movement
    bool is_static = false;     // Static colliders don't move
    
    // Get world-space bounds given entity position
    AABB GetWorldBounds(const vec3& entity_pos) const {
        vec3 world_center = entity_pos + offset;
        
        switch (type) {
            case ColliderType::Sphere:
                return AABB{
                    world_center - vec3{radius, radius, radius},
                    world_center + vec3{radius, radius, radius}
                };
            case ColliderType::Box:
                return AABB{
                    world_center - half_extents,
                    world_center + half_extents
                };
            case ColliderType::Capsule: {
                float half_height = height * 0.5f;
                return AABB{
                    world_center - vec3{radius, half_height, radius},
                    world_center + vec3{radius, half_height, radius}
                };
            }
            default:
                return AABB{world_center, world_center};
        }
    }
    
    // Get capsule in world space
    Capsule GetWorldCapsule(const vec3& entity_pos) const {
        return Capsule{
            entity_pos + offset,
            radius,
            height
        };
    }
    
    // Get sphere in world space
    Sphere GetWorldSphere(const vec3& entity_pos) const {
        return Sphere{entity_pos + offset, radius};
    }
};

/*
 * RigidbodyComponent - For dynamic physics objects (simulated by Jolt)
 * 
 * Used for props, debris, thrown objects - NOT player movement.
 * Add this component to enable physics simulation.
 */
struct RigidbodyComponent {
    // Velocity (read from physics, can be set to add impulse)
    vec3 velocity{0, 0, 0};
    vec3 angular_velocity{0, 0, 0};
    
    // Physical properties (used when creating Jolt body)
    float mass = 1.0f;              // Mass in kg
    float friction = 0.5f;          // Surface friction [0-1]
    float restitution = 0.3f;       // Bounciness [0-1]
    float linear_damping = 0.05f;   // Velocity damping (weighty feel)
    float angular_damping = 0.05f;  // Rotation damping
    float gravity_factor = 1.0f;    // Gravity multiplier (0 = no gravity)
    
    bool is_kinematic = false;      // Kinematic = moved by code, not physics
    bool use_ccd = false;           // Continuous collision detection (for fast objects)
    
    // Legacy: accumulated forces for this frame (deprecated, use Jolt)
    vec3 force{0, 0, 0};
    vec3 torque{0, 0, 0};
    
    void AddForce(const vec3& f) { force = force + f; }
    void AddImpulse(const vec3& impulse) { velocity = velocity + impulse * (1.0f / mass); }
    void AddTorque(const vec3& t) { torque = torque + t; }
};

} // namespace action
