#pragma once

/*
 * BlendTreeSystem — procedural animation composition.
 *
 * Provides four node types that can be arbitrarily nested to express any
 * per-entity blending policy without modifying AnimationSystem:
 *
 *   Clip    — leaf: evaluates a single AnimationClip at an advancing time.
 *   Lerp1D  — interpolates two child poses by one float parameter.
 *             e.g. blend walk↔run smoothly by speed.
 *   Lerp2D  — blends N directional clips by inverse-distance weighting in
 *             a 2D parameter space.
 *             e.g. 8-directional locomotion blend (velocity.x / velocity.z).
 *   Additive — adds child[1]'s delta-from-rest on top of child[0]'s base
 *             pose, scaled by additive_weight.
 *             e.g. breathing / wounds / upper-body aim-offset layered over
 *             lower-body locomotion.
 *
 * Usage:
 *   1. Add BlendTreeComponent and AnimationPlayerComponent to an entity.
 *   2. Fill BlendTreeComponent::tree with nodes; set skeleton_name.
 *   3. Each frame, update tree.float_params / bool_params with game state.
 *   4. BlendTreeSystem writes the resulting pose into
 *      AnimationPlayerComponent::local_transforms (so IKSystem can still
 *      run on top).
 */

#include "core/types.h"
#include "animation/animation_player.h"
#include "animation/animation_library.h"
#include "gameplay/ecs/ecs.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace action {

// -------------------------------------------------------------------------
// BlendNodeType
// -------------------------------------------------------------------------
enum class BlendNodeType : u8 {
    Clip,       // Leaf: one clip, advancing time
    Lerp1D,     // Two children blended by a single float parameter
    Lerp2D,     // N directional clips blended in 2D parameter space
    Additive,   // Additive layer: base + delta*weight
};

// Directional sample for Lerp2D nodes.
struct BlendSample2D {
    std::string clip_name;
    float       px = 0.f;   // X-axis parameter position
    float       py = 0.f;   // Y-axis parameter position
};

// -------------------------------------------------------------------------
// BlendNode — element of the flat node pool
// -------------------------------------------------------------------------
struct BlendNode {
    BlendNodeType  type  = BlendNodeType::Clip;

    // ---- Clip leaf ----
    std::string  clip_name;
    float        time    = 0.f;     // Current playback time (seconds)
    float        speed   = 1.f;     // Playback speed multiplier
    bool         loop    = true;

    // ---- Lerp1D ----
    // Blends children[0] (param <= param_low) toward children[1] (param >= param_high).
    std::string  param_name;
    float        param_low  = 0.f;
    float        param_high = 1.f;

    // ---- Lerp2D ----
    std::string  param_x;           // Float parameter for the X axis
    std::string  param_y;           // Float parameter for the Y axis
    std::vector<BlendSample2D> samples_2d;
    float        time_2d = 0.f;     // Shared playback time for all 2D samples

    // ---- Additive ----
    // children[0] = base pose, children[1] = additive layer.
    float        additive_weight = 1.f;

    // Children: indices into BlendTree::nodes.
    std::vector<u32> children;
};

// -------------------------------------------------------------------------
// BlendTree — complete tree for one entity
// -------------------------------------------------------------------------
struct BlendTree {
    std::vector<BlendNode> nodes;   // Flat node pool
    u32 root = 0;                   // Index of the root node

    // Runtime parameters written by game code each frame.
    std::unordered_map<std::string, float> float_params;
    std::unordered_map<std::string, bool>  bool_params;

    float GetFloat(const std::string& name, float def = 0.f) const {
        auto it = float_params.find(name);
        return (it != float_params.end()) ? it->second : def;
    }
};

// -------------------------------------------------------------------------
// BlendTreeComponent — ECS component
// -------------------------------------------------------------------------
struct BlendTreeComponent {
    BlendTree   tree;
    std::string skeleton_name;      // Must match an entry in AnimationLibrary
};

// -------------------------------------------------------------------------
// BlendTreeSystem — ECS system
// -------------------------------------------------------------------------
class BlendTreeSystem : public System {
public:
    BlendTreeSystem(ECS* ecs, AnimationLibrary* library)
        : m_ecs(ecs), m_library(library) {}

    // Called each frame by ECS::Update(dt), after AnimationSystem.
    // Overwrites AnimationPlayerComponent::local_transforms and
    // skinning_matrices for entities that own a BlendTreeComponent.
    void Update(float dt) override;

private:
    // Recursively evaluate node at 'node_idx'; writes result into out_local.
    // Returns false if the node or any required asset is unavailable.
    bool EvaluateNode(u32 node_idx,
                      float dt,
                      BlendTree& tree,
                      const Skeleton& skel,
                      std::vector<mat4>& out_local) const;

    // In-place element-wise lerp: out_a = lerp(out_a, b, t).
    static void BlendPoses(std::vector<mat4>& out_a,
                            const std::vector<mat4>& b,
                            float t,
                            u32 bone_count);

    ECS*              m_ecs     = nullptr;
    AnimationLibrary* m_library = nullptr;
};

} // namespace action
