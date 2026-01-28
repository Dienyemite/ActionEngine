#pragma once

/*
 * Input System - Godot-style action-based input
 * 
 * Central include for input system
 */

#include "input_map.h"

namespace action {

/*
 * Common input action presets for typical game setups
 */
inline void SetupDefaultActions(InputMap& input) {
    // Movement
    input.AddAction("move_forward");
    input.ActionAddEvent("move_forward", InputEvent::FromKey(Key::W));
    input.ActionAddEvent("move_forward", InputEvent::FromKey(Key::Up));
    
    input.AddAction("move_back");
    input.ActionAddEvent("move_back", InputEvent::FromKey(Key::S));
    input.ActionAddEvent("move_back", InputEvent::FromKey(Key::Down));
    
    input.AddAction("move_left");
    input.ActionAddEvent("move_left", InputEvent::FromKey(Key::A));
    input.ActionAddEvent("move_left", InputEvent::FromKey(Key::Left));
    
    input.AddAction("move_right");
    input.ActionAddEvent("move_right", InputEvent::FromKey(Key::D));
    input.ActionAddEvent("move_right", InputEvent::FromKey(Key::Right));
    
    // Actions
    input.AddAction("jump");
    input.ActionAddEvent("jump", InputEvent::FromKey(Key::Space));
    input.ActionAddEvent("jump", InputEvent::FromGamepadButton(GamepadButton::A));
    
    input.AddAction("sprint");
    input.ActionAddEvent("sprint", InputEvent::FromKey(Key::Shift));
    input.ActionAddEvent("sprint", InputEvent::FromGamepadButton(GamepadButton::LeftStick));
    
    input.AddAction("crouch");
    input.ActionAddEvent("crouch", InputEvent::FromKey(Key::Control));
    input.ActionAddEvent("crouch", InputEvent::FromGamepadButton(GamepadButton::B));
    
    input.AddAction("interact");
    input.ActionAddEvent("interact", InputEvent::FromKey(Key::E));
    input.ActionAddEvent("interact", InputEvent::FromGamepadButton(GamepadButton::X));
    
    // Combat
    input.AddAction("attack");
    input.ActionAddEvent("attack", InputEvent::FromMouseButton(Key::MouseLeft));
    input.ActionAddEvent("attack", InputEvent::FromGamepadButton(GamepadButton::RightTrigger));
    
    input.AddAction("block");
    input.ActionAddEvent("block", InputEvent::FromMouseButton(Key::MouseRight));
    input.ActionAddEvent("block", InputEvent::FromGamepadButton(GamepadButton::LeftTrigger));
    
    input.AddAction("aim");
    input.ActionAddEvent("aim", InputEvent::FromMouseButton(Key::MouseRight));
    input.ActionAddEvent("aim", InputEvent::FromGamepadButton(GamepadButton::LeftTrigger));
    
    // UI
    input.AddAction("ui_accept");
    input.ActionAddEvent("ui_accept", InputEvent::FromKey(Key::Enter));
    input.ActionAddEvent("ui_accept", InputEvent::FromKey(Key::Space));
    input.ActionAddEvent("ui_accept", InputEvent::FromGamepadButton(GamepadButton::A));
    
    input.AddAction("ui_cancel");
    input.ActionAddEvent("ui_cancel", InputEvent::FromKey(Key::Escape));
    input.ActionAddEvent("ui_cancel", InputEvent::FromGamepadButton(GamepadButton::B));
    
    input.AddAction("ui_up");
    input.ActionAddEvent("ui_up", InputEvent::FromKey(Key::Up));
    input.ActionAddEvent("ui_up", InputEvent::FromKey(Key::W));
    
    input.AddAction("ui_down");
    input.ActionAddEvent("ui_down", InputEvent::FromKey(Key::Down));
    input.ActionAddEvent("ui_down", InputEvent::FromKey(Key::S));
    
    input.AddAction("ui_left");
    input.ActionAddEvent("ui_left", InputEvent::FromKey(Key::Left));
    input.ActionAddEvent("ui_left", InputEvent::FromKey(Key::A));
    
    input.AddAction("ui_right");
    input.ActionAddEvent("ui_right", InputEvent::FromKey(Key::Right));
    input.ActionAddEvent("ui_right", InputEvent::FromKey(Key::D));
    
    // Menu/System
    input.AddAction("pause");
    input.ActionAddEvent("pause", InputEvent::FromKey(Key::Escape));
    input.ActionAddEvent("pause", InputEvent::FromGamepadButton(GamepadButton::Start));
    
    input.AddAction("menu");
    input.ActionAddEvent("menu", InputEvent::FromKey(Key::Tab));
    input.ActionAddEvent("menu", InputEvent::FromGamepadButton(GamepadButton::Back));
    
    // Camera
    input.AddAction("camera_zoom_in");
    input.ActionAddEvent("camera_zoom_in", InputEvent::FromKey(Key::PageUp));
    
    input.AddAction("camera_zoom_out");
    input.ActionAddEvent("camera_zoom_out", InputEvent::FromKey(Key::PageDown));
}

} // namespace action
