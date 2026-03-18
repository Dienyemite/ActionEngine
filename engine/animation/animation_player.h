#pragma once

/*
 * AnimationPlayerComponent — ECS component that drives clip playback on a skeleton.
 * AnimationSystem          — ECS system that evaluates the component each frame.
 *
 * Workflow per entity:
 *   1. AnimationSystem locates the Skeleton by name in AnimationLibrary.
 *   2. It evaluates AnimationStateMachineComponent (if present) to pick the
 *      current clip and blend parameters, then advances the player time.
 *   3. AnimationClip::Sample() fills a local-transform buffer (one mat4/bone).
 *   4. Skeleton::ComputeSkinningMatrices() produces the final skinning matrices.
 *   5. CPUSkinMesh() applies those matrices to the SkinnedMeshData bind-pose
 *      vertices, producing posed_vertices.
 *   6. The renderer reads posed_vertices when drawing the entity's mesh.
 *
 * TODO(animation): Replace step 5-6 with GPU skinning once the pipeline has a
 * per-draw bone-matrix SSBO.
 */

#include "core/types.h"
#include "animation/skeleton.h"
#include "animation/animation_clip.h"
#include "animation/animation_library.h"
#include "gameplay/ecs/ecs.h"
#include <string>
#include <vector>

namespace action {

class AssetManager;  // Forward declaration

// -------------------------------------------------------------------------
// AnimationPlayerComponent — ECS component
// -------------------------------------------------------------------------
struct AnimationPlayerComponent {
    std::string skeleton_name;      // Key into AnimationLibrary
    std::string current_clip;       // Clip to play right now
    float       time         = 0.0f;
    float       speed        = 1.0f;
    bool        playing      = true;
    bool        loop         = true;

    // Read-only: updated by AnimationSystem each frame.
    // Per-bone LOCAL transforms (one per bone, in bone's parent space).
    // Written by AnimationSystem after clip sampling; read+modified by IKSystem
    // before skinning matrices are recomputed.
    std::vector<mat4> local_transforms;

    // One mat4 per bone: skinning_matrices[i] = world_pose[i] * inv_bind_pose[i].
    // The renderer / CPU skinner reads these.
    std::vector<mat4> skinning_matrices;

    // How much of the blend to the next clip is complete [0,1].
    // Used by AnimationSystem when a state-machine transition is active.
    float blend_weight       = 0.0f;
    std::string blend_clip;     // Second clip during a crossfade
    float blend_time         = 0.0f;
};

// -------------------------------------------------------------------------
// AnimationSystem — ECS system
// -------------------------------------------------------------------------
class AnimationSystem : public System {
public:
    explicit AnimationSystem(ECS* ecs, AnimationLibrary* library)
        : m_ecs(ecs), m_library(library) {}

    // Optional: provide an AssetManager so CPU-skinned vertices can be
    // uploaded to the entity's GPU mesh buffer each frame.
    void SetAssets(AssetManager* assets) { m_assets = assets; }

    // Called each frame by ECS::Update(dt).
    void Update(float dt) override;

private:
    // Evaluate the state machine (if present) and update the player's current
    // clip / blend state.  Returns true if the player was modified.
    bool EvaluateStateMachine(Entity entity,
                              AnimationPlayerComponent& player,
                              float dt) const;

    // Fill 'out_local' to skeleton size with rest-pose transforms,
    // then overwrite the animated bones from the clip at time t.
    void SampleClip(const Skeleton& skel,
                    const AnimationClip& clip,
                    float t,
                    std::vector<mat4>& out_local) const;

    ECS*              m_ecs     = nullptr;
    AnimationLibrary* m_library = nullptr;
    AssetManager*     m_assets  = nullptr;  // Optional; enables GPU vertex upload
};

} // namespace action
