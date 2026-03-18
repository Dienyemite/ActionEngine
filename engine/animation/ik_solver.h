#pragma once

/*
 * IKSystem — Two-Bone IK and Look-At IK solvers.
 *
 * Runs AFTER AnimationSystem each frame.  For any entity that has both an
 * AnimationPlayerComponent and an IKComponent, the solvers modify
 * AnimationPlayerComponent::local_transforms in-place, then recompute
 * skinning_matrices from the corrected pose.
 *
 * Two-Bone IK (root → mid → tip):
 *   Analytical solution using the law of cosines and a pole vector.
 *   Use cases: foot-plant IK, hand contact with surfaces/bosses, ledge grips.
 *
 * Look-At IK (single bone):
 *   Rotates one bone so its local forward axis tracks a world-space point.
 *   Use cases: head/neck look-at, eye aim, spine lean-toward a target.
 */

#include "core/types.h"
#include "animation/animation_player.h"
#include "animation/animation_library.h"
#include "gameplay/ecs/ecs.h"
#include <string>
#include <vector>

namespace action {

// -------------------------------------------------------------------------
// IKType
// -------------------------------------------------------------------------
enum class IKType : u8 {
    TwoBone,  // 3-bone chain (root, mid, tip) driven to a world-space target
    LookAt,   // Single bone aimed at a target using its local forward axis
};

// -------------------------------------------------------------------------
// IKChain — one constraint attached to a skeleton
// -------------------------------------------------------------------------
struct IKChain {
    IKType      type          = IKType::TwoBone;

    // Bone names resolved once against the entity's Skeleton.
    // TwoBone:  root_bone = upper limb, mid_bone = lower limb, tip_bone = effector
    // LookAt:   only root_bone is used
    std::string root_bone;
    std::string mid_bone;
    std::string tip_bone;

    // World-space target updated by game code each frame before IKSystem::Update.
    vec3  target_world    = {};

    // TwoBone: world-space point in the desired bend plane (pole vector).
    // Leave at zero to use the current mid-bone direction as a fallback.
    vec3  pole_world      = {};

    // LookAt: bone-local axis that should face the target.
    vec3  forward_axis    = { 0.f, 0.f, 1.f };

    // IK blend weight [0,1].  0 = fully animated pose, 1 = fully IK-driven.
    float weight          = 1.0f;

    // Set enabled = true to activate this chain.
    bool  enabled         = false;

    // Cached resolve results; -1 = unresolved.
    mutable i32 root_idx  = -1;
    mutable i32 mid_idx   = -1;
    mutable i32 tip_idx   = -1;
};

// -------------------------------------------------------------------------
// IKComponent — ECS component
// -------------------------------------------------------------------------
struct IKComponent {
    std::vector<IKChain> chains;
};

// -------------------------------------------------------------------------
// IKSystem — ECS system
// -------------------------------------------------------------------------
class IKSystem : public System {
public:
    IKSystem(ECS* ecs, AnimationLibrary* library)
        : m_ecs(ecs), m_library(library) {}

    // Called each frame by ECS::Update(dt), after AnimationSystem.
    void Update(float dt) override;

private:
    // Analytical two-bone IK using law of cosines.
    void SolveTwoBone(IKChain& chain,
                      const Skeleton& skel,
                      std::vector<mat4>& local_xforms) const;

    // Single-bone look-at IK.
    void SolveLookAt(IKChain& chain,
                     const Skeleton& skel,
                     std::vector<mat4>& local_xforms) const;

    // Quaternion that rotates unit vector 'from' onto unit vector 'to'.
    static quat RotationBetween(const vec3& from, const vec3& to);

    ECS*              m_ecs     = nullptr;
    AnimationLibrary* m_library = nullptr;
};

} // namespace action
