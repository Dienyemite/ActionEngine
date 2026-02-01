#pragma once

#include "scripting/script.h"
#include "scripting/script_system.h"
#include "input/input.h"
#include "physics/character_controller.h"
#include "physics/physics_world.h"
#include "physics/collision_shapes.h"

namespace action {

/*
 * PlayerController - Physics-enabled player movement for action games
 * 
 * This script provides snappy 3D character movement with:
 * - WASD movement relative to camera
 * - Jump with Space (with coyote time and jump buffering)
 * - Sprint with Shift
 * - Instant direction changes (no momentum fighting)
 * - Ground detection via CharacterController
 */
class PlayerController : public Script {
public:
    SCRIPT_CLASS(PlayerController)
    
    // Movement settings
    float move_speed = 8.0f;
    float sprint_multiplier = 1.6f;
    float jump_force = 10.0f;
    float air_control = 0.8f;       // Multiplier for air movement
    
    // Camera angles (set externally or calculated)
    float camera_yaw = 0.0f;
    
    void OnStart() override {
        Log("PlayerController started!");
        
        // Ensure entity has CharacterControllerComponent
        if (!HasComponent<CharacterControllerComponent>()) {
            Log("Adding CharacterControllerComponent");
            auto& cc = AddComponent<CharacterControllerComponent>();
            cc.config.height = 1.8f;
            cc.config.radius = 0.4f;
            cc.config.step_height = 0.35f;
            cc.config.slope_limit = 50.0f;
            cc.coyote_time = 0.12f;         // Generous coyote time for action games
            cc.jump_buffer_duration = 0.15f; // Jump buffer for responsive controls
        }
    }
    
    void OnUpdate(float dt) override {
        Input* input = GetInput();
        auto* cc = GetComponent<CharacterControllerComponent>();
        if (!cc) return;
        
        // Calculate camera-relative movement directions
        float yaw_rad = camera_yaw * DEG_TO_RAD;
        vec3 cam_forward{std::sin(yaw_rad), 0, std::cos(yaw_rad)};
        vec3 cam_right{std::cos(yaw_rad), 0, -std::sin(yaw_rad)};
        
        // Input direction
        vec3 input_dir{0, 0, 0};
        if (input->IsKeyDown(Key::W)) input_dir = input_dir + cam_forward;
        if (input->IsKeyDown(Key::S)) input_dir = input_dir - cam_forward;
        if (input->IsKeyDown(Key::D)) input_dir = input_dir + cam_right;
        if (input->IsKeyDown(Key::A)) input_dir = input_dir - cam_right;
        
        // Normalize if moving diagonally
        if (input_dir.length_sq() > 0.01f) {
            input_dir = input_dir.normalized();
        }
        
        // Calculate speed
        float current_speed = move_speed;
        if (input->IsKeyDown(Key::Shift)) {
            current_speed *= sprint_multiplier;
        }
        
        // Apply air control reduction
        if (!cc->ground.grounded) {
            current_speed *= air_control;
        }
        
        // Set movement (instant, no acceleration - for snappy controls)
        cc->Move(input_dir * current_speed * dt);
        
        // Jump
        if (input->IsKeyPressed(Key::Space)) {
            cc->Jump(jump_force);
        }
        
        // Landing effects
        if (cc->JustLanded()) {
            // Could trigger landing sound/particles here
        }
    }
    
    void OnCollisionEnter(Entity other) override {
        Log("Collided with entity!");
    }
    
    void OnDestroy() override {
        Log("PlayerController destroyed");
    }
};

/*
 * SimplePlayerController - Non-physics player movement (legacy)
 * 
 * Uses simple velocity + ground check at y=0.
 * Good for prototyping before physics is set up.
 */
class SimplePlayerController : public Script {
public:
    SCRIPT_CLASS(SimplePlayerController)
    
    float move_speed = 5.0f;
    float sprint_multiplier = 2.0f;
    float jump_force = 8.0f;
    float gravity = 20.0f;
    float acceleration = 15.0f;
    float deceleration = 10.0f;
    
    void OnStart() override {
        m_velocity = vec3{0, 0, 0};
        m_grounded = true;
    }
    
    void OnUpdate(float dt) override {
        Input* input = GetInput();
        
        vec3 cam_forward = GetForward();
        vec3 cam_right = GetRight();
        
        cam_forward.y = 0;
        cam_forward = cam_forward.length_sq() > 0.01f ? cam_forward.normalized() : vec3{0, 0, 1};
        cam_right.y = 0;
        cam_right = cam_right.length_sq() > 0.01f ? cam_right.normalized() : vec3{1, 0, 0};
        
        vec3 input_dir{0, 0, 0};
        if (input->IsKeyDown(Key::W)) input_dir = input_dir + cam_forward;
        if (input->IsKeyDown(Key::S)) input_dir = input_dir - cam_forward;
        if (input->IsKeyDown(Key::D)) input_dir = input_dir + cam_right;
        if (input->IsKeyDown(Key::A)) input_dir = input_dir - cam_right;
        
        if (input_dir.length_sq() > 0.01f) {
            input_dir = input_dir.normalized();
        }
        
        float current_speed = move_speed;
        if (input->IsKeyDown(Key::Shift)) {
            current_speed *= sprint_multiplier;
        }
        
        vec3 target_velocity = input_dir * current_speed;
        
        float accel = (input_dir.length_sq() > 0.01f) ? acceleration : deceleration;
        m_velocity.x = lerp(m_velocity.x, target_velocity.x, accel * dt);
        m_velocity.z = lerp(m_velocity.z, target_velocity.z, accel * dt);
        
        if (input->IsKeyPressed(Key::Space) && m_grounded) {
            m_velocity.y = jump_force;
            m_grounded = false;
        }
        
        if (!m_grounded) {
            m_velocity.y -= gravity * dt;
        }
        
        Translate(m_velocity * dt);
        
        vec3 pos = GetPosition();
        if (pos.y <= 0.0f) {
            pos.y = 0.0f;
            SetPosition(pos);
            m_velocity.y = 0.0f;
            m_grounded = true;
        }
    }
    
private:
    vec3 m_velocity{0, 0, 0};
    bool m_grounded = true;
    
    static float lerp(float a, float b, float t) {
        if (t > 1.0f) t = 1.0f;
        return a + (b - a) * t;
    }
};

/*
 * RotatingObject - Simple script that rotates an object
 */
class RotatingObject : public Script {
public:
    SCRIPT_CLASS(RotatingObject)
    
    vec3 rotation_speed{0, 45, 0};  // Degrees per second
    
    void OnUpdate(float dt) override {
        Rotate(rotation_speed * dt);
    }
};

/*
 * LookAtTarget - Makes object always face a target
 */
class LookAtTarget : public Script {
public:
    SCRIPT_CLASS(LookAtTarget)
    
    std::string target_name = "Player";
    float smooth_speed = 5.0f;
    
    void OnStart() override {
        m_target = FindEntityByName(target_name);
        if (m_target == INVALID_ENTITY) {
            LogWarning("Target '" + target_name + "' not found");
        }
    }
    
    void OnUpdate(float dt) override {
        if (m_target == INVALID_ENTITY) return;
        
        auto* target_transform = GetECS()->GetComponent<TransformComponent>(m_target);
        if (!target_transform) return;
        
        LookAt(target_transform->position);
    }
    
private:
    Entity m_target = INVALID_ENTITY;
};

/*
 * Lifetime - Destroys entity after a specified duration
 */
class Lifetime : public Script {
public:
    SCRIPT_CLASS(Lifetime)
    
    float duration = 3.0f;
    
    void OnStart() override {
        m_timer = duration;
    }
    
    void OnUpdate(float dt) override {
        m_timer -= dt;
        if (m_timer <= 0.0f) {
            DestroySelf();
        }
    }
    
private:
    float m_timer = 0.0f;
};

/*
 * TriggerZone - Physics-enabled trigger volume
 * 
 * Use this to detect when entities enter/exit a zone.
 * Configure with ColliderComponent + Trigger layer.
 */
class TriggerZone : public Script {
public:
    SCRIPT_CLASS(TriggerZone)
    
    std::string on_enter_message = "";
    bool destroy_on_trigger = false;
    CollisionLayer trigger_layer = CollisionLayer::Player;
    
    void OnUpdate(float dt) override {
        auto* physics = GetPhysics();
        if (!physics) return;
        
        vec3 pos = GetPosition();
        auto overlaps = physics->OverlapSphere(pos, 1.0f, static_cast<uint32_t>(trigger_layer));
        
        for (auto entity : overlaps) {
            if (entity != GetEntity()) {
                OnTriggered(entity);
                if (destroy_on_trigger) {
                    DestroySelf();
                    return;
                }
            }
        }
    }
    
    virtual void OnTriggered(Entity other) {
        Log("Trigger activated by entity!");
    }
};

/*
 * HealthPickup - Example collectible that uses physics overlap
 */
class HealthPickup : public Script {
public:
    SCRIPT_CLASS(HealthPickup)
    
    float pickup_radius = 1.0f;
    float heal_amount = 25.0f;
    float bob_speed = 2.0f;
    float bob_height = 0.3f;
    
    void OnStart() override {
        m_start_y = GetPosition().y;
        m_time = 0.0f;
    }
    
    void OnUpdate(float dt) override {
        // Bob up and down
        m_time += dt;
        vec3 pos = GetPosition();
        pos.y = m_start_y + std::sin(m_time * bob_speed) * bob_height;
        SetPosition(pos);
        
        // Rotate for visual effect
        Rotate(vec3{0, 90 * dt, 0});
        
        // Check for player overlap using physics
        if (auto overlaps = OverlapSphere(pos, pickup_radius, static_cast<uint32_t>(CollisionLayer::Player)); !overlaps.empty()) {
            // In a real game, you'd give health to the player here
            Log("Health pickup collected! +" + std::to_string(static_cast<int>(heal_amount)));
            DestroySelf();
        }
    }
    
private:
    float m_start_y = 0.0f;
    float m_time = 0.0f;
};

/*
 * Projectile - Physics-enabled projectile with raycast hit detection
 */
class Projectile : public Script {
public:
    SCRIPT_CLASS(Projectile)
    
    float speed = 30.0f;
    float max_distance = 100.0f;
    float damage = 10.0f;
    uint32_t hit_mask = static_cast<uint32_t>(CollisionLayer::Enemy) | 
                        static_cast<uint32_t>(CollisionLayer::Environment);
    
    void OnStart() override {
        m_direction = GetForward();
        m_distance_traveled = 0.0f;
    }
    
    void OnUpdate(float dt) override {
        float move_distance = speed * dt;
        vec3 pos = GetPosition();
        
        // Raycast ahead to detect hits
        RaycastHit hit;
        if (Raycast(pos, m_direction, move_distance + 0.1f, hit, hit_mask)) {
            OnHit(hit);
            DestroySelf();
            return;
        }
        
        // Move forward
        Translate(m_direction * move_distance);
        m_distance_traveled += move_distance;
        
        // Destroy if traveled too far
        if (m_distance_traveled >= max_distance) {
            DestroySelf();
        }
    }
    
    virtual void OnHit(const RaycastHit& hit) {
        Log("Projectile hit entity at distance: " + std::to_string(hit.distance));
        // In a real game, apply damage to hit.entity here
    }
    
private:
    vec3 m_direction{0, 0, 1};
    float m_distance_traveled = 0.0f;
};

// Registration function - call this from game initialization
inline void RegisterBuiltinScripts() {
    REGISTER_SCRIPT(PlayerController);
    REGISTER_SCRIPT(SimplePlayerController);
    REGISTER_SCRIPT(RotatingObject);
    REGISTER_SCRIPT(LookAtTarget);
    REGISTER_SCRIPT(Lifetime);
    REGISTER_SCRIPT(TriggerZone);
    REGISTER_SCRIPT(HealthPickup);
    REGISTER_SCRIPT(Projectile);
}

} // namespace action
