#pragma once

#include "core/types.h"
#include "core/math/math.h"
#include "collision_shapes.h"
#include "gameplay/ecs/ecs.h"

namespace action {

// Forward declaration
class PhysicsWorld;

/*
 * CharacterController - Snappy, responsive movement for action games
 * 
 * NOT physics-driven. This is a kinematic controller that:
 * - Uses capsule collision
 * - Handles slopes and steps
 * - Supports wall sliding
 * - Provides ground detection
 * - Allows for fast direction changes (no momentum fighting player input)
 * 
 * Designed for games like Devil May Cry, Bayonetta, God of War
 * where player input should immediately translate to movement.
 */

struct GroundInfo {
    bool grounded = false;
    vec3 normal{0, 1, 0};
    vec3 point{0, 0, 0};
    float slope_angle = 0.0f;
    Entity ground_entity = INVALID_ENTITY;
};

struct CharacterControllerConfig {
    // Capsule shape
    float radius = 0.4f;
    float height = 1.8f;
    
    // Movement
    float step_height = 0.35f;      // Max height to auto-step
    float slope_limit = 50.0f;      // Max walkable slope in degrees
    float skin_width = 0.02f;       // Small buffer to prevent tunneling
    
    // Ground detection
    float ground_check_distance = 0.1f;
    
    // Collision
    CollisionLayer layer = CollisionLayer::Player;
    CollisionLayer mask = CollisionLayer::Environment | CollisionLayer::Default;
};

/*
 * CharacterControllerComponent - ECS component for character movement
 */
struct CharacterControllerComponent {
    // Configuration
    CharacterControllerConfig config;
    
    // Current state
    vec3 velocity{0, 0, 0};         // Current velocity (for gravity, jumps)
    vec3 move_input{0, 0, 0};       // Desired movement this frame
    
    // Ground state
    GroundInfo ground;
    bool was_grounded = false;
    float time_since_grounded = 0.0f;
    
    // Coyote time (allows jumping shortly after leaving ground)
    float coyote_time = 0.1f;
    bool CanCoyoteJump() const { return time_since_grounded < coyote_time; }
    
    // Jump buffer (allows pressing jump slightly before landing)
    float jump_buffer_time = 0.0f;
    float jump_buffer_duration = 0.15f;
    
    // Movement helpers
    void Move(const vec3& direction) {
        move_input = move_input + direction;
    }
    
    void Jump(float force) {
        if (ground.grounded || CanCoyoteJump()) {
            velocity.y = force;
            time_since_grounded = coyote_time; // Consume coyote time
        } else {
            // Buffer the jump input
            jump_buffer_time = jump_buffer_duration;
        }
    }
    
    // Check if we just landed (for landing effects, sounds)
    bool JustLanded() const {
        return ground.grounded && !was_grounded;
    }
    
    // Check if we just left ground (for fall detection)
    bool JustLeftGround() const {
        return !ground.grounded && was_grounded;
    }
    
    // Get effective move direction (projected onto ground plane if grounded)
    vec3 GetGroundedMoveDirection() const {
        if (!ground.grounded) return move_input;
        
        // Project movement onto ground plane
        vec3 right = cross(move_input, ground.normal);
        if (right.length_sq() < 0.0001f) return move_input;
        
        return cross(ground.normal, right).normalized() * move_input.length();
    }
};

/*
 * CharacterController - The actual movement processor
 * 
 * Call Move() with desired world-space movement vector.
 * Handles collision, sliding, stepping automatically.
 */
class CharacterController {
public:
    CharacterController() = default;
    
    // Initialize with physics world reference
    void Initialize(PhysicsWorld* world, ECS* ecs);
    
    // Process all character controllers
    void Update(float dt);
    
    // Move a specific character
    // Returns actual movement after collision
    vec3 MoveCharacter(Entity entity, CharacterControllerComponent& controller,
                       TransformComponent& transform, const vec3& movement, float dt);
    
private:
    // Core movement phases
    void UpdateGroundState(CharacterControllerComponent& controller,
                           const vec3& position);
    
    vec3 SlideMove(const Capsule& capsule, const vec3& movement,
                   CollisionLayer mask, int max_iterations = 4);
    
    bool TryStepUp(const Capsule& capsule, const vec3& movement,
                   float step_height, CollisionLayer mask,
                   vec3& out_position);
    
    // Collision queries
    bool CapsuleCast(const Capsule& capsule, const vec3& direction, 
                     float distance, CollisionLayer mask,
                     vec3& hit_point, vec3& hit_normal, float& hit_distance);
    
    bool CheckCapsuleOverlap(const Capsule& capsule, CollisionLayer mask);
    
    void ResolvePenetration(Capsule& capsule, CollisionLayer mask);
    
    PhysicsWorld* m_world = nullptr;
    ECS* m_ecs = nullptr;
};

} // namespace action
