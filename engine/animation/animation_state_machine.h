#pragma once

/*
 * AnimationStateMachine — parameter-driven state graph.
 *
 * Each state references an animation clip by name.  Transitions fire when all
 * of their conditions evaluate to true.  A single active transition blends the
 * outgoing and incoming clips over blend_duration seconds.
 *
 * The state machine is stored as an ECS component (AnimationStateMachineComponent)
 * and evaluated by AnimationSystem::Update() each frame, which then drives the
 * AnimationPlayerComponent on the same entity.
 *
 * Simplified model (intentionally):
 *   - Flat array of states — no hierarchical state nesting.
 *   - Linear blend only (no blend trees).
 *   - Transitions are evaluated in declaration order; first match wins.
 */

#include "core/types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace action {

// -------------------------------------------------------------------------
// Transition condition
// -------------------------------------------------------------------------
struct AnimCondition {
    enum class Op {
        FloatEqual,     FloatNotEqual,
        FloatGreater,   FloatLess,
        FloatGreaterEq, FloatLessEq,
        BoolTrue,       BoolFalse,
        Trigger,        // True once then auto-cleared
    };

    std::string parameter;  // Name in the parameter table
    Op          op          = Op::BoolTrue;
    float       threshold   = 0.0f;
};

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------
struct AnimState {
    std::string name;
    std::string clip_name;  // AnimationClip name in AnimationLibrary
    float       speed       = 1.0f;
    bool        loop        = true;
};

// -------------------------------------------------------------------------
// Transition
// -------------------------------------------------------------------------
struct AnimTransition {
    u32   from_state     = 0;           // Index into states[]
    u32   to_state       = 0;
    float blend_duration = 0.1f;        // Seconds
    bool  can_interrupt  = false;       // Interrupt an in-progress transition?

    std::vector<AnimCondition> conditions;

    // Evaluate all conditions against the parameter tables.
    bool Evaluate(const std::unordered_map<std::string, float>& floats,
                  const std::unordered_map<std::string, bool>&  bools,
                  std::unordered_map<std::string, bool>&         triggers) const;
};

// -------------------------------------------------------------------------
// AnimationStateMachineComponent — ECS component
// -------------------------------------------------------------------------
struct AnimationStateMachineComponent {
    std::vector<AnimState>      states;
    std::vector<AnimTransition> transitions;

    // Current playback state
    u32   current_state_index  = 0;
    bool  in_transition        = false;
    u32   target_state_index   = 0;
    float transition_time      = 0.0f;  // Elapsed blend time
    float transition_duration  = 0.0f;  // Total blend time (from transition.blend_duration)

    // Parameter tables updated by game code each frame.
    std::unordered_map<std::string, float> float_params;
    std::unordered_map<std::string, bool>  bool_params;
    std::unordered_map<std::string, bool>  triggers;    // Auto-cleared after evaluation

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    void AddState(const std::string& name, const std::string& clip, float speed = 1.0f, bool loop = true) {
        states.push_back({name, clip, speed, loop});
    }

    void AddTransition(u32 from, u32 to, float blend = 0.1f) {
        transitions.push_back({from, to, blend, false, {}});
    }

    // The back of transitions[] is the most-recently added: add a condition to it.
    AnimationStateMachineComponent& WithCondition(AnimCondition cond) {
        if (!transitions.empty()) transitions.back().conditions.push_back(cond);
        return *this;
    }

    // Shorthand parameter setters
    void SetFloat(const std::string& name, float v) { float_params[name] = v; }
    void SetBool (const std::string& name, bool  v) { bool_params[name]  = v; }
    void SetTrigger(const std::string& name)         { triggers[name]    = true; }

    float GetFloat(const std::string& name, float def = 0.0f) const {
        auto it = float_params.find(name);
        return it != float_params.end() ? it->second : def;
    }
    bool GetBool(const std::string& name, bool def = false) const {
        auto it = bool_params.find(name);
        return it != bool_params.end() ? it->second : def;
    }

    const AnimState& GetCurrentState() const {
        if (current_state_index < states.size()) return states[current_state_index];
        static AnimState empty{};
        return empty;
    }

    // Find a state index by name; returns UINT32_MAX if not found.
    u32 FindState(const std::string& name) const {
        for (u32 i = 0; i < (u32)states.size(); ++i) {
            if (states[i].name == name) return i;
        }
        return UINT32_MAX;
    }
};

} // namespace action
