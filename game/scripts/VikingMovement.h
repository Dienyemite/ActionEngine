#pragma once
#include "scripting/script.h"
#include "platform/platform.h"  // Input, Key, MouseState

namespace action {

/*
 * VikingMovement — WASD test movement for the vikingmedieval mesh
 *
 * Controls:
 *   W / S        — move forward / backward
 *   A / D        — strafe left / right
 *   Shift        — sprint (2x speed)
 *   Right Mouse  — hold to rotate with mouse (yaw only)
 *
 * Attach this script to the vikingmedieval entity in the Inspector.
 * No physics or CharacterController required.
 */
class VikingMovement : public Script {
public:
    SCRIPT_CLASS(VikingMovement)

    // Tweak these in code or expose via editor later
    float move_speed    = 6.0f;   // units / second
    float sprint_mult   = 2.0f;
    float mouse_sens    = 0.15f;  // degrees per pixel
    float gravity       = 12.0f;  // simple ground gravity (units / s^2)

    void OnStart() override {
        m_yaw   = 0.0f;  // start facing +Z
        m_vel_y = 0.0f;
        Log("VikingMovement started — WASD to move, hold RMB to look.");
    }

    void OnUpdate(float dt) override {
        Input* input = GetInput();
        if (!input) return;

        // ---- Mouse look (only while RMB held) ----
        const MouseState& mouse = input->GetMouse();
        if (input->IsKeyDown(Key::MouseRight)) {
            m_yaw += mouse.delta_x * mouse_sens;
        }

        // ---- Build movement directions from yaw ----
        const float yaw_rad = m_yaw * DEG_TO_RAD;
        const vec3 forward{ std::sin(yaw_rad), 0.0f,  std::cos(yaw_rad) };
        const vec3 right  { std::cos(yaw_rad), 0.0f, -std::sin(yaw_rad) };

        // ---- Input direction ----
        vec3 move_dir{ 0.0f, 0.0f, 0.0f };
        if (input->IsKeyDown(Key::W)) move_dir = move_dir + forward;
        if (input->IsKeyDown(Key::S)) move_dir = move_dir - forward;
        if (input->IsKeyDown(Key::D)) move_dir = move_dir + right;
        if (input->IsKeyDown(Key::A)) move_dir = move_dir - right;

        if (move_dir.length_sq() > 0.001f)
            move_dir = move_dir.normalized();

        // ---- Speed ----
        const float speed = move_speed * (input->IsKeyDown(Key::Shift) ? sprint_mult : 1.0f);

        // ---- Simple Y gravity (snap to y = 0 ground) ----
        vec3 pos = GetPosition();
        if (pos.y > 0.0f) {
            m_vel_y -= gravity * dt;
        } else {
            pos.y   = 0.0f;
            m_vel_y = 0.0f;
        }

        // ---- Apply movement ----
        pos = pos + move_dir * speed * dt;
        pos.y += m_vel_y * dt;
        SetPosition(pos);

        // ---- Face movement direction (when no mouse look) ----
        if (!input->IsKeyDown(Key::MouseRight) && move_dir.length_sq() > 0.001f) {
            m_yaw = std::atan2(move_dir.x, move_dir.z) * RAD_TO_DEG;
        }

        // Apply yaw rotation to the entity
        SetRotation(quat::from_euler(0.0f, m_yaw * DEG_TO_RAD, 0.0f));
    }

private:
    float m_yaw   = 0.0f;
    float m_vel_y = 0.0f;
};

} // namespace action
