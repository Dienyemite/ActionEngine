#pragma once

#include "core/types.h"
#include "platform/platform.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace action {

/*
 * InputMap - Action-based input system (Godot-style)
 * 
 * Features:
 * - Map actions to multiple input sources (keyboard, mouse, gamepad)
 * - Dead zones and sensitivity
 * - Input strength for analog inputs
 * - Action events (pressed, released, held)
 * - Input layers/contexts (gameplay, UI, vehicle, etc.)
 */

// Input source types
enum class InputType : u8 {
    Key,
    MouseButton,
    MouseMotion,
    MouseWheel,
    GamepadButton,
    GamepadAxis
};

// Gamepad buttons (Xbox layout)
enum class GamepadButton : u8 {
    A, B, X, Y,
    LeftShoulder, RightShoulder,
    LeftTrigger, RightTrigger,  // As buttons (pressed threshold)
    Back, Start, Guide,
    LeftStick, RightStick,      // Stick press
    DPadUp, DPadDown, DPadLeft, DPadRight,
    Count
};

// Gamepad axes
enum class GamepadAxis : u8 {
    LeftX, LeftY,
    RightX, RightY,
    LeftTrigger, RightTrigger,
    Count
};

// Axis direction for treating axis as button
enum class AxisDirection : i8 {
    Negative = -1,
    Any = 0,
    Positive = 1
};

// Single input event definition
struct InputEvent {
    InputType type = InputType::Key;
    
    // Key/Button
    Key key = Key::Unknown;
    GamepadButton gamepad_button = GamepadButton::A;
    
    // Axis
    GamepadAxis axis = GamepadAxis::LeftX;
    AxisDirection axis_direction = AxisDirection::Any;
    
    // Modifiers (for keyboard)
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    
    // Matching
    bool Matches(const InputEvent& other) const;
    
    // Factory functions
    static InputEvent FromKey(Key k, bool ctrl = false, bool shift = false, bool alt = false);
    static InputEvent FromMouseButton(Key button);
    static InputEvent FromGamepadButton(GamepadButton btn);
    static InputEvent FromGamepadAxis(GamepadAxis axis, AxisDirection dir = AxisDirection::Any);
};

// Action configuration
struct InputAction {
    std::string name;
    std::vector<InputEvent> events;
    
    float dead_zone = 0.2f;      // For analog inputs
    bool echo = false;           // Generate events while held
    
    void AddEvent(const InputEvent& event) { events.push_back(event); }
    void RemoveEvent(const InputEvent& event);
    void ClearEvents() { events.clear(); }
};

// Action state
struct ActionState {
    bool pressed = false;        // Pressed this frame
    bool released = false;       // Released this frame
    bool held = false;           // Currently held
    float strength = 0.0f;       // 0.0 to 1.0 (analog strength)
    float raw_strength = 0.0f;   // Before dead zone
};

// Input context for layered input
class InputContext {
public:
    explicit InputContext(const std::string& name) : m_name(name) {}
    
    const std::string& GetName() const { return m_name; }
    
    void AddAction(const std::string& action_name);
    void RemoveAction(const std::string& action_name);
    bool HasAction(const std::string& action_name) const;
    
    const std::vector<std::string>& GetActions() const { return m_actions; }
    
private:
    std::string m_name;
    std::vector<std::string> m_actions;
};

class InputMap {
public:
    InputMap() = default;
    ~InputMap() = default;
    
    bool Initialize(Input& platform_input);
    void Shutdown();
    
    // Called each frame to update action states
    void Update();
    
    // ===== Action Management =====
    
    void AddAction(const std::string& name);
    void RemoveAction(const std::string& name);
    bool HasAction(const std::string& name) const;
    InputAction* GetAction(const std::string& name);
    
    // Add input events to action
    void ActionAddEvent(const std::string& action, const InputEvent& event);
    void ActionEraseEvent(const std::string& action, const InputEvent& event);
    void ActionEraseEvents(const std::string& action);
    
    // Get all actions
    std::vector<std::string> GetActions() const;
    
    // ===== Action Queries =====
    
    // Check if action was just pressed this frame
    bool IsActionPressed(const std::string& action) const;
    
    // Check if action was just released this frame
    bool IsActionReleased(const std::string& action) const;
    
    // Check if action is currently held
    bool IsActionHeld(const std::string& action) const;
    
    // Get analog strength (0.0 to 1.0)
    float GetActionStrength(const std::string& action) const;
    
    // Get raw strength (before dead zone)
    float GetActionRawStrength(const std::string& action) const;
    
    // Get axis value (-1.0 to 1.0 from two actions)
    float GetAxis(const std::string& negative_action, const std::string& positive_action) const;
    
    // Get 2D vector from 4 actions
    vec2 GetVector(const std::string& neg_x, const std::string& pos_x,
                   const std::string& neg_y, const std::string& pos_y) const;
    
    // ===== Contexts =====
    
    void AddContext(const std::string& name);
    void RemoveContext(const std::string& name);
    InputContext* GetContext(const std::string& name);
    
    void PushContext(const std::string& name);
    void PopContext();
    const std::string& GetCurrentContext() const;
    
    // Check if action is active in current context
    bool IsActionActive(const std::string& action) const;
    
    // ===== Gamepad =====
    
    void SetGamepadDeadZone(float dead_zone) { m_gamepad_dead_zone = dead_zone; }
    float GetGamepadDeadZone() const { return m_gamepad_dead_zone; }
    
    bool IsGamepadConnected() const { return m_gamepad_connected; }
    float GetGamepadAxis(GamepadAxis axis) const;
    bool IsGamepadButtonPressed(GamepadButton button) const;
    
    // ===== Mouse =====
    
    vec2 GetMousePosition() const;
    vec2 GetMouseDelta() const;
    float GetMouseWheelDelta() const;
    
    // ===== Serialization =====
    
    void SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);
    
private:
    void UpdateActionState(const std::string& name, InputAction& action);
    float ApplyDeadZone(float value, float dead_zone) const;
    
    Input* m_platform_input = nullptr;
    
    std::unordered_map<std::string, InputAction> m_actions;
    std::unordered_map<std::string, ActionState> m_states;
    std::unordered_map<std::string, ActionState> m_prev_states;
    
    std::unordered_map<std::string, InputContext> m_contexts;
    std::vector<std::string> m_context_stack;
    
    float m_gamepad_dead_zone = 0.2f;
    bool m_gamepad_connected = false;
    
    std::array<float, static_cast<size_t>(GamepadAxis::Count)> m_gamepad_axes{};
    std::array<bool, static_cast<size_t>(GamepadButton::Count)> m_gamepad_buttons{};
};

} // namespace action
