#include "input_map.h"
#include "core/logging.h"
#include <algorithm>
#include <fstream>
#include <cmath>

namespace action {

// ===== InputEvent =====

bool InputEvent::Matches(const InputEvent& other) const {
    if (type != other.type) return false;
    
    switch (type) {
        case InputType::Key:
            return key == other.key && ctrl == other.ctrl && 
                   shift == other.shift && alt == other.alt;
        case InputType::MouseButton:
            return key == other.key;  // Mouse buttons stored in key
        case InputType::GamepadButton:
            return gamepad_button == other.gamepad_button;
        case InputType::GamepadAxis:
            return axis == other.axis && axis_direction == other.axis_direction;
        default:
            return false;
    }
}

InputEvent InputEvent::FromKey(Key k, bool ctrl, bool shift, bool alt) {
    InputEvent e;
    e.type = InputType::Key;
    e.key = k;
    e.ctrl = ctrl;
    e.shift = shift;
    e.alt = alt;
    return e;
}

InputEvent InputEvent::FromMouseButton(Key button) {
    InputEvent e;
    e.type = InputType::MouseButton;
    e.key = button;
    return e;
}

InputEvent InputEvent::FromGamepadButton(GamepadButton btn) {
    InputEvent e;
    e.type = InputType::GamepadButton;
    e.gamepad_button = btn;
    return e;
}

InputEvent InputEvent::FromGamepadAxis(GamepadAxis axis, AxisDirection dir) {
    InputEvent e;
    e.type = InputType::GamepadAxis;
    e.axis = axis;
    e.axis_direction = dir;
    return e;
}

// ===== InputAction =====

void InputAction::RemoveEvent(const InputEvent& event) {
    events.erase(
        std::remove_if(events.begin(), events.end(),
            [&event](const InputEvent& e) { return e.Matches(event); }),
        events.end()
    );
}

// ===== InputContext =====

void InputContext::AddAction(const std::string& action_name) {
    if (!HasAction(action_name)) {
        m_actions.push_back(action_name);
    }
}

void InputContext::RemoveAction(const std::string& action_name) {
    m_actions.erase(
        std::remove(m_actions.begin(), m_actions.end(), action_name),
        m_actions.end()
    );
}

bool InputContext::HasAction(const std::string& action_name) const {
    return std::find(m_actions.begin(), m_actions.end(), action_name) != m_actions.end();
}

// ===== InputMap =====

bool InputMap::Initialize(Input& platform_input) {
    m_platform_input = &platform_input;
    
    // Add default context
    AddContext("default");
    PushContext("default");
    
    LOG_INFO("InputMap initialized");
    return true;
}

void InputMap::Shutdown() {
    m_actions.clear();
    m_states.clear();
    m_contexts.clear();
    m_context_stack.clear();
    m_platform_input = nullptr;
    LOG_INFO("InputMap shutdown");
}

void InputMap::Update() {
    if (!m_platform_input) return;
    
    // Save previous states
    m_prev_states = m_states;
    
    // Update each action
    for (auto& [name, action] : m_actions) {
        UpdateActionState(name, action);
    }
}

void InputMap::UpdateActionState(const std::string& name, InputAction& action) {
    ActionState& state = m_states[name];
    const ActionState& prev = m_prev_states[name];
    
    float max_strength = 0.0f;
    float max_raw = 0.0f;
    bool is_pressed = false;
    
    for (const auto& event : action.events) {
        float strength = 0.0f;
        float raw = 0.0f;
        
        switch (event.type) {
            case InputType::Key: {
                // Check modifiers
                bool ctrl_ok = !event.ctrl || m_platform_input->IsKeyDown(Key::Control);
                bool shift_ok = !event.shift || m_platform_input->IsKeyDown(Key::Shift);
                bool alt_ok = !event.alt || m_platform_input->IsKeyDown(Key::Alt);
                
                if (ctrl_ok && shift_ok && alt_ok && m_platform_input->IsKeyDown(event.key)) {
                    strength = 1.0f;
                    raw = 1.0f;
                }
                break;
            }
            
            case InputType::MouseButton:
                if (m_platform_input->IsKeyDown(event.key)) {
                    strength = 1.0f;
                    raw = 1.0f;
                }
                break;
            
            case InputType::GamepadButton:
                if (m_gamepad_buttons[static_cast<size_t>(event.gamepad_button)]) {
                    strength = 1.0f;
                    raw = 1.0f;
                }
                break;
            
            case InputType::GamepadAxis: {
                float axis_value = m_gamepad_axes[static_cast<size_t>(event.axis)];
                raw = std::abs(axis_value);
                
                // Check direction
                bool matches = false;
                switch (event.axis_direction) {
                    case AxisDirection::Positive:
                        matches = axis_value > action.dead_zone;
                        break;
                    case AxisDirection::Negative:
                        matches = axis_value < -action.dead_zone;
                        break;
                    case AxisDirection::Any:
                        matches = std::abs(axis_value) > action.dead_zone;
                        break;
                }
                
                if (matches) {
                    strength = ApplyDeadZone(std::abs(axis_value), action.dead_zone);
                }
                break;
            }
            
            default:
                break;
        }
        
        max_strength = std::max(max_strength, strength);
        max_raw = std::max(max_raw, raw);
        
        if (strength > 0) {
            is_pressed = true;
        }
    }
    
    // Update state
    state.held = is_pressed;
    state.pressed = is_pressed && !prev.held;
    state.released = !is_pressed && prev.held;
    state.strength = max_strength;
    state.raw_strength = max_raw;
}

float InputMap::ApplyDeadZone(float value, float dead_zone) const {
    if (value <= dead_zone) return 0.0f;
    return (value - dead_zone) / (1.0f - dead_zone);
}

// ===== Action Management =====

void InputMap::AddAction(const std::string& name) {
    if (m_actions.count(name)) return;
    
    InputAction action;
    action.name = name;
    m_actions[name] = action;
    m_states[name] = ActionState{};
}

void InputMap::RemoveAction(const std::string& name) {
    m_actions.erase(name);
    m_states.erase(name);
}

bool InputMap::HasAction(const std::string& name) const {
    return m_actions.count(name) > 0;
}

InputAction* InputMap::GetAction(const std::string& name) {
    auto it = m_actions.find(name);
    return it != m_actions.end() ? &it->second : nullptr;
}

void InputMap::ActionAddEvent(const std::string& action, const InputEvent& event) {
    if (auto* a = GetAction(action)) {
        a->AddEvent(event);
    }
}

void InputMap::ActionEraseEvent(const std::string& action, const InputEvent& event) {
    if (auto* a = GetAction(action)) {
        a->RemoveEvent(event);
    }
}

void InputMap::ActionEraseEvents(const std::string& action) {
    if (auto* a = GetAction(action)) {
        a->ClearEvents();
    }
}

std::vector<std::string> InputMap::GetActions() const {
    std::vector<std::string> result;
    result.reserve(m_actions.size());
    for (const auto& [name, _] : m_actions) {
        result.push_back(name);
    }
    return result;
}

// ===== Action Queries =====

bool InputMap::IsActionPressed(const std::string& action) const {
    auto it = m_states.find(action);
    return it != m_states.end() && it->second.pressed;
}

bool InputMap::IsActionReleased(const std::string& action) const {
    auto it = m_states.find(action);
    return it != m_states.end() && it->second.released;
}

bool InputMap::IsActionHeld(const std::string& action) const {
    auto it = m_states.find(action);
    return it != m_states.end() && it->second.held;
}

float InputMap::GetActionStrength(const std::string& action) const {
    auto it = m_states.find(action);
    return it != m_states.end() ? it->second.strength : 0.0f;
}

float InputMap::GetActionRawStrength(const std::string& action) const {
    auto it = m_states.find(action);
    return it != m_states.end() ? it->second.raw_strength : 0.0f;
}

float InputMap::GetAxis(const std::string& negative_action, const std::string& positive_action) const {
    float neg = GetActionStrength(negative_action);
    float pos = GetActionStrength(positive_action);
    return pos - neg;
}

vec2 InputMap::GetVector(const std::string& neg_x, const std::string& pos_x,
                         const std::string& neg_y, const std::string& pos_y) const {
    return vec2{
        GetAxis(neg_x, pos_x),
        GetAxis(neg_y, pos_y)
    };
}

// ===== Contexts =====

void InputMap::AddContext(const std::string& name) {
    m_contexts.emplace(name, InputContext(name));
}

void InputMap::RemoveContext(const std::string& name) {
    m_contexts.erase(name);
    m_context_stack.erase(
        std::remove(m_context_stack.begin(), m_context_stack.end(), name),
        m_context_stack.end()
    );
}

InputContext* InputMap::GetContext(const std::string& name) {
    auto it = m_contexts.find(name);
    return it != m_contexts.end() ? &it->second : nullptr;
}

void InputMap::PushContext(const std::string& name) {
    if (m_contexts.count(name)) {
        m_context_stack.push_back(name);
    }
}

void InputMap::PopContext() {
    if (!m_context_stack.empty()) {
        m_context_stack.pop_back();
    }
}

const std::string& InputMap::GetCurrentContext() const {
    static std::string empty;
    return m_context_stack.empty() ? empty : m_context_stack.back();
}

bool InputMap::IsActionActive(const std::string& action) const {
    if (m_context_stack.empty()) return true;  // No context = all active
    
    for (auto it = m_context_stack.rbegin(); it != m_context_stack.rend(); ++it) {
        auto ctx = m_contexts.find(*it);
        if (ctx != m_contexts.end() && ctx->second.HasAction(action)) {
            return true;
        }
    }
    return false;
}

// ===== Gamepad =====

float InputMap::GetGamepadAxis(GamepadAxis axis) const {
    return m_gamepad_axes[static_cast<size_t>(axis)];
}

bool InputMap::IsGamepadButtonPressed(GamepadButton button) const {
    return m_gamepad_buttons[static_cast<size_t>(button)];
}

// ===== Mouse =====

vec2 InputMap::GetMousePosition() const {
    if (!m_platform_input) return vec2{0, 0};
    const MouseState& mouse = m_platform_input->GetMouse();
    return vec2{static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
}

vec2 InputMap::GetMouseDelta() const {
    if (!m_platform_input) return vec2{0, 0};
    const MouseState& mouse = m_platform_input->GetMouse();
    return vec2{static_cast<float>(mouse.delta_x), static_cast<float>(mouse.delta_y)};
}

float InputMap::GetMouseWheelDelta() const {
    if (!m_platform_input) return 0.0f;
    return m_platform_input->GetMouse().scroll_delta;
}

// ===== Serialization =====

void InputMap::SaveToFile(const std::string& path) const {
    // Will be implemented with serialization system
    LOG_INFO("Saving input map to: %s", path.c_str());
}

bool InputMap::LoadFromFile(const std::string& path) {
    // Will be implemented with serialization system
    LOG_INFO("Loading input map from: %s", path.c_str());
    return true;
}

} // namespace action
