#pragma once

#include "scripting/script.h"
#include "scripting/script_system.h"
#include "input/input.h"

namespace action {

/*
 * PlayerController - Example script demonstrating the scripting system
 * 
 * This script provides basic 3D character movement with:
 * - WASD movement relative to camera
 * - Jump with Space
 * - Sprint with Shift
 * - Smooth acceleration/deceleration
 */
class PlayerController : public Script {
public:
    SCRIPT_CLASS(PlayerController)
    
    // Movement settings (can be tweaked per instance)
    float move_speed = 5.0f;
    float sprint_multiplier = 2.0f;
    float jump_force = 8.0f;
    float gravity = 20.0f;
    float acceleration = 15.0f;
    float deceleration = 10.0f;
    
    void OnStart() override {
        Log("PlayerController started!");
        m_velocity = vec3{0, 0, 0};
        m_grounded = true;
    }
    
    void OnUpdate(float dt) override {
        Input* input = GetInput();
        
        // Get camera for movement direction
        // Note: In a real game you'd get camera forward from your camera system
        vec3 cam_forward = GetForward();
        vec3 cam_right = GetRight();
        
        // Flatten to horizontal plane
        cam_forward.y = 0;
        cam_forward = normalize(cam_forward);
        cam_right.y = 0;
        cam_right = normalize(cam_right);
        
        // Input direction
        vec3 input_dir{0, 0, 0};
        if (input->IsKeyDown(Key::W)) input_dir = input_dir + cam_forward;
        if (input->IsKeyDown(Key::S)) input_dir = input_dir - cam_forward;
        if (input->IsKeyDown(Key::D)) input_dir = input_dir + cam_right;
        if (input->IsKeyDown(Key::A)) input_dir = input_dir - cam_right;
        
        // Normalize if moving diagonally
        if (length(input_dir) > 0.1f) {
            input_dir = normalize(input_dir);
        }
        
        // Calculate target velocity
        float current_speed = move_speed;
        if (input->IsKeyDown(Key::Shift)) {
            current_speed *= sprint_multiplier;
        }
        
        vec3 target_velocity = input_dir * current_speed;
        
        // Smooth acceleration/deceleration (only horizontal)
        float accel = (length(input_dir) > 0.1f) ? acceleration : deceleration;
        m_velocity.x = lerp(m_velocity.x, target_velocity.x, accel * dt);
        m_velocity.z = lerp(m_velocity.z, target_velocity.z, accel * dt);
        
        // Jump
        if (input->IsKeyPressed(Key::Space) && m_grounded) {
            m_velocity.y = jump_force;
            m_grounded = false;
        }
        
        // Apply gravity
        if (!m_grounded) {
            m_velocity.y -= gravity * dt;
        }
        
        // Move
        Translate(m_velocity * dt);
        
        // Simple ground check (assumes ground at y=0)
        vec3 pos = GetPosition();
        if (pos.y <= 0.0f) {
            pos.y = 0.0f;
            SetPosition(pos);
            m_velocity.y = 0.0f;
            m_grounded = true;
        }
    }
    
    void OnCollisionEnter(Entity other) override {
        Log("Collided with entity!");
    }
    
    void OnDestroy() override {
        Log("PlayerController destroyed");
    }
    
private:
    vec3 m_velocity{0, 0, 0};
    bool m_grounded = true;
    
    // Simple lerp helper
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

// Registration function - call this from game initialization
inline void RegisterBuiltinScripts() {
    REGISTER_SCRIPT(PlayerController);
    REGISTER_SCRIPT(RotatingObject);
    REGISTER_SCRIPT(LookAtTarget);
    REGISTER_SCRIPT(Lifetime);
}

} // namespace action
