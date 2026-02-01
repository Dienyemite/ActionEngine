#include "character_controller.h"
#include "physics_world.h"
#include "core/logging.h"
#include "gameplay/ecs/ecs.h"
#include <algorithm>

namespace action {

void CharacterController::Initialize(PhysicsWorld* world, ECS* ecs) {
    m_world = world;
    m_ecs = ecs;
    LOG_INFO("[CharacterController] Initialized");
}

void CharacterController::Update(float dt) {
    // Process all character controllers
    m_ecs->ForEach<CharacterControllerComponent, TransformComponent>(
        [this, dt](Entity entity, CharacterControllerComponent& controller, TransformComponent& transform) {
            // Store previous grounded state
            controller.was_grounded = controller.ground.grounded;
            
            // Apply gravity if not grounded
            if (!controller.ground.grounded) {
                controller.velocity = controller.velocity + m_world->GetGravity() * dt;
                controller.time_since_grounded += dt;
            } else {
                controller.time_since_grounded = 0.0f;
                // Dampen vertical velocity when grounded
                if (controller.velocity.y < 0) {
                    controller.velocity.y = 0.0f;
                }
            }
            
            // Check for buffered jump
            if (controller.jump_buffer_time > 0.0f) {
                controller.jump_buffer_time -= dt;
                if (controller.ground.grounded) {
                    // Execute buffered jump
                    controller.velocity.y = 8.0f;  // Default jump force
                    controller.jump_buffer_time = 0.0f;
                }
            }
            
            // Calculate total movement
            vec3 movement = controller.move_input + controller.velocity * dt;
            
            // Move character with collision
            vec3 actual_movement = MoveCharacter(entity, controller, transform, movement, dt);
            
            // Clear move input for next frame
            controller.move_input = {0, 0, 0};
            
            // Update ground state
            UpdateGroundState(controller, transform.position);
        }
    );
}

vec3 CharacterController::MoveCharacter(Entity entity, CharacterControllerComponent& controller,
                                          TransformComponent& transform, const vec3& movement, float dt) {
    if (movement.length_sq() < EPSILON) return {0, 0, 0};
    
    const auto& config = controller.config;
    
    // Build capsule at current position
    Capsule capsule;
    capsule.center = transform.position + vec3{0, config.height * 0.5f, 0};
    capsule.radius = config.radius;
    capsule.height = config.height;
    
    // Separate horizontal and vertical movement for better control
    vec3 horizontal = vec3{movement.x, 0, movement.z};
    vec3 vertical = vec3{0, movement.y, 0};
    
    vec3 final_position = transform.position;
    
    // Handle horizontal movement with step detection
    if (horizontal.length_sq() > EPSILON) {
        float horizontal_dist = horizontal.length();
        vec3 horizontal_dir = horizontal.normalized();
        
        // Try direct horizontal movement
        SweepHit hit = m_world->CapsuleCast(capsule, horizontal_dir, horizontal_dist + config.skin_width,
                                             config.mask, entity);
        
        if (hit) {
            // Check if we can step up
            vec3 step_pos;
            if (TryStepUp(capsule, horizontal_dir * horizontal_dist, config.step_height, config.mask, step_pos)) {
                final_position = step_pos;
                capsule.center = final_position + vec3{0, config.height * 0.5f, 0};
            } else {
                // Slide along the obstacle
                float safe_dist = hit.time * horizontal_dist - config.skin_width;
                if (safe_dist > 0) {
                    final_position = final_position + horizontal_dir * safe_dist;
                    capsule.center = final_position + vec3{0, config.height * 0.5f, 0};
                }
                
                // Calculate remaining movement
                float remaining = horizontal_dist - safe_dist;
                if (remaining > EPSILON) {
                    // Slide along the wall
                    vec3 slide_dir = horizontal_dir - hit.normal * dot(horizontal_dir, hit.normal);
                    if (slide_dir.length_sq() > EPSILON) {
                        slide_dir = slide_dir.normalized();
                        float slide_dist = remaining * (1.0f - std::abs(dot(horizontal_dir, hit.normal)));
                        
                        SweepHit slide_hit = m_world->CapsuleCast(capsule, slide_dir, slide_dist + config.skin_width,
                                                                   config.mask, entity);
                        
                        float slide_safe = slide_hit ? slide_hit.time * slide_dist - config.skin_width : slide_dist;
                        if (slide_safe > 0) {
                            final_position = final_position + slide_dir * slide_safe;
                            capsule.center = final_position + vec3{0, config.height * 0.5f, 0};
                        }
                    }
                }
                
                // If hit wall, zero out velocity in that direction
                float vel_into_wall = dot(controller.velocity, hit.normal);
                if (vel_into_wall < 0) {
                    controller.velocity = controller.velocity - hit.normal * vel_into_wall;
                }
            }
        } else {
            // No collision, move freely
            final_position = final_position + horizontal;
            capsule.center = final_position + vec3{0, config.height * 0.5f, 0};
        }
    }
    
    // Handle vertical movement
    if (vertical.length_sq() > EPSILON) {
        float vertical_dist = std::abs(vertical.y);
        vec3 vertical_dir = vertical.y > 0 ? vec3{0, 1, 0} : vec3{0, -1, 0};
        
        SweepHit hit = m_world->CapsuleCast(capsule, vertical_dir, vertical_dist + config.skin_width,
                                             config.mask, entity);
        
        if (hit) {
            float safe_dist = hit.time * vertical_dist - config.skin_width;
            if (safe_dist > 0) {
                final_position = final_position + vertical_dir * safe_dist;
            }
            
            // Zero out vertical velocity on collision
            if ((vertical.y < 0 && hit.normal.y > 0.5f) ||  // Hit ground
                (vertical.y > 0 && hit.normal.y < -0.5f)) { // Hit ceiling
                controller.velocity.y = 0.0f;
            }
        } else {
            final_position = final_position + vertical;
        }
    }
    
    vec3 actual_movement = final_position - transform.position;
    transform.position = final_position;
    
    return actual_movement;
}

void CharacterController::UpdateGroundState(CharacterControllerComponent& controller,
                                              const vec3& position) {
    const auto& config = controller.config;
    
    // Cast down from capsule base
    vec3 ray_origin = position + vec3{0, config.radius + 0.01f, 0};
    float ray_dist = config.ground_check_distance + 0.02f;
    
    RaycastHit hit = m_world->Raycast(ray_origin, {0, -1, 0}, ray_dist, config.mask);
    
    if (hit) {
        controller.ground.grounded = true;
        controller.ground.normal = hit.normal;
        controller.ground.point = hit.point;
        controller.ground.ground_entity = hit.entity;
        
        // Calculate slope angle
        float dot_up = dot(hit.normal, vec3{0, 1, 0});
        controller.ground.slope_angle = std::acos(Clamp(dot_up, -1.0f, 1.0f)) * RAD_TO_DEG;
        
        // Check if slope is too steep
        if (controller.ground.slope_angle > config.slope_limit) {
            controller.ground.grounded = false;
        }
    } else {
        controller.ground.grounded = false;
        controller.ground.normal = {0, 1, 0};
        controller.ground.slope_angle = 0.0f;
        controller.ground.ground_entity = INVALID_ENTITY;
    }
}

bool CharacterController::TryStepUp(const Capsule& capsule, const vec3& movement,
                                     float step_height, CollisionLayer mask,
                                     vec3& out_position) {
    // Step 1: Move up by step height
    Capsule raised = capsule;
    raised.center = raised.center + vec3{0, step_height, 0};
    
    // Check if raised position is clear
    if (m_world->OverlapCapsule(raised, mask).size() > 0) {
        return false;  // Blocked above
    }
    
    // Step 2: Move forward in raised position
    float forward_dist = movement.length();
    vec3 forward_dir = movement.normalized();
    
    SweepHit forward_hit = m_world->CapsuleCast(raised, forward_dir, forward_dist, mask);
    
    if (forward_hit && forward_hit.time < 0.1f) {
        return false;  // Still blocked
    }
    
    float actual_forward = forward_hit ? forward_hit.time * forward_dist : forward_dist;
    raised.center = raised.center + forward_dir * actual_forward;
    
    // Step 3: Move down to find ground
    SweepHit down_hit = m_world->CapsuleCast(raised, {0, -1, 0}, step_height + 0.1f, mask);
    
    if (down_hit) {
        float step_down = down_hit.time * (step_height + 0.1f);
        out_position = raised.center - vec3{0, capsule.height * 0.5f + step_down, 0};
        return true;
    }
    
    return false;
}

bool CharacterController::CapsuleCast(const Capsule& capsule, const vec3& direction,
                                       float distance, CollisionLayer mask,
                                       vec3& hit_point, vec3& hit_normal, float& hit_distance) {
    SweepHit hit = m_world->CapsuleCast(capsule, direction, distance, mask);
    
    if (hit) {
        hit_point = hit.point;
        hit_normal = hit.normal;
        hit_distance = hit.time * distance;
        return true;
    }
    
    return false;
}

bool CharacterController::CheckCapsuleOverlap(const Capsule& capsule, CollisionLayer mask) {
    return m_world->OverlapCapsule(capsule, mask).size() > 0;
}

void CharacterController::ResolvePenetration(Capsule& capsule, CollisionLayer mask) {
    const int max_iterations = 4;
    
    for (int i = 0; i < max_iterations; ++i) {
        auto overlapping = m_world->OverlapCapsule(capsule, mask);
        if (overlapping.empty()) break;
        
        for (Entity entity : overlapping) {
            auto* transform = m_ecs->GetComponent<TransformComponent>(entity);
            auto* collider = m_ecs->GetComponent<ColliderComponent>(entity);
            if (!transform || !collider) continue;
            
            // Push capsule out based on collider type
            vec3 push_dir{0, 1, 0};
            float push_dist = 0.01f;
            
            if (collider->type == ColliderType::Box) {
                AABB box = collider->GetWorldBounds(transform->position);
                vec3 closest;
                closest.x = Clamp(capsule.center.x, box.min.x, box.max.x);
                closest.y = Clamp(capsule.center.y, box.min.y, box.max.y);
                closest.z = Clamp(capsule.center.z, box.min.z, box.max.z);
                
                push_dir = (capsule.center - closest);
                if (push_dir.length_sq() > EPSILON) {
                    push_dir = push_dir.normalized();
                }
                push_dist = capsule.radius - (capsule.center - closest).length() + 0.01f;
            }
            
            capsule.center = capsule.center + push_dir * push_dist;
        }
    }
}

} // namespace action
