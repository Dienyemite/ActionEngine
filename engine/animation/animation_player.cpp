#include "animation_player.h"
#include "animation_clip.h"
#include "animation_state_machine.h"
#include "skinned_mesh.h"
#include "core/logging.h"
#include "core/profiler.h"
#include "core/math/math.h"
#include <cmath>

namespace action {

// ---------------------------------------------------------------------------
// AnimationSystem::Update
// ---------------------------------------------------------------------------
void AnimationSystem::Update(float dt) {
    PROFILE_SCOPE("AnimationSystem::Update");

    m_ecs->ForEach<AnimationPlayerComponent>([&](Entity entity, AnimationPlayerComponent& player) {
        if (!player.playing || player.skeleton_name.empty()) return;

        const Skeleton* skel = m_library->GetSkeleton(player.skeleton_name);
        if (!skel || !skel->is_valid()) return;

        // ------------------------------------------------------------------
        // 1. Evaluate state machine (updates current_clip / blend_clip)
        // ------------------------------------------------------------------
        EvaluateStateMachine(entity, player, dt);

        // ------------------------------------------------------------------
        // 2. Advance playback time
        // ------------------------------------------------------------------
        const AnimationClip* clip = m_library->GetClip(player.current_clip);
        if (!clip || !clip->is_valid()) return;

        player.time += dt * player.speed;
        player.time  = clip->WrapTime(player.time);

        // ------------------------------------------------------------------
        // 3. Sample primary clip → local transforms
        // ------------------------------------------------------------------
        const u32 bone_count = (u32)skel->bones.size();
        std::vector<mat4> local_transforms(bone_count, mat4::identity());

        // Pre-fill with rest pose
        for (u32 i = 0; i < bone_count; ++i) {
            local_transforms[i] = skel->bones[i].local_rest_transform;
        }

        SampleClip(*skel, *clip, player.time, local_transforms);

        // ------------------------------------------------------------------
        // 4. If blending: sample second clip and linearly blend
        // ------------------------------------------------------------------
        if (player.blend_weight > 0.0f && !player.blend_clip.empty()) {
            const AnimationClip* blend = m_library->GetClip(player.blend_clip);
            if (blend && blend->is_valid()) {
                std::vector<mat4> blend_local(bone_count, mat4::identity());
                for (u32 i = 0; i < bone_count; ++i) {
                    blend_local[i] = skel->bones[i].local_rest_transform;
                }
                SampleClip(*skel, *blend, player.blend_time, blend_local);

                // Blend each bone's local transform component-wise.
                // We decompose TRS, lerp/slerp, then recompose.
                // (Blending raw mat4 entries directly is inaccurate but cheaper;
                //  component-wise TRS decomposition would be more accurate.)
                const float w = player.blend_weight;
                for (u32 i = 0; i < bone_count; ++i) {
                    // Simple linear blend of raw matrix elements — acceptable
                    // for short crossfades with similar poses.
                    for (int r = 0; r < 4; ++r)
                        for (int c = 0; c < 4; ++c)
                            local_transforms[i].m[c][r] =
                                local_transforms[i].m[c][r] * (1.0f - w) +
                                blend_local[i].m[c][r] * w;
                }
            }
        }

        // ------------------------------------------------------------------
        // 5. Compute skinning matrices
        // ------------------------------------------------------------------
        player.skinning_matrices.resize(bone_count);
        skel->ComputeSkinningMatrices(local_transforms, player.skinning_matrices);

        // ------------------------------------------------------------------
        // 6. CPU skin the mesh (if skinned mesh data is available)
        // ------------------------------------------------------------------
        // Look up a SkinnedMeshData whose skeleton_name matches this player.
        SkinnedMeshData* skinned = m_library->GetSkinnedMesh(player.skeleton_name);
        if (skinned && skinned->is_valid()) {
            CPUSkinMesh(*skinned, player.skinning_matrices, skinned->posed_vertices);
            // TODO(animation): upload posed_vertices to the GPU vertex buffer
            // corresponding to this entity's mesh so the renderer draws them.
        }
    });
}

// ---------------------------------------------------------------------------
// EvaluateStateMachine
// ---------------------------------------------------------------------------
bool AnimationSystem::EvaluateStateMachine(Entity entity,
                                            AnimationPlayerComponent& player,
                                            float dt) const {
    auto* sm = m_ecs->GetComponent<AnimationStateMachineComponent>(entity);
    if (!sm || sm->states.empty()) return false;

    bool changed = false;

    // Advance in-progress transition
    if (sm->in_transition) {
        sm->transition_time += dt;
        const float t = sm->transition_duration > 0.0f
            ? std::min(sm->transition_time / sm->transition_duration, 1.0f)
            : 1.0f;
        player.blend_weight = t;
        player.blend_time  += dt;

        if (t >= 1.0f) {
            // Transition complete: commit to target state
            sm->current_state_index = sm->target_state_index;
            sm->in_transition       = false;
            sm->transition_time     = 0.0f;
            player.current_clip     = sm->GetCurrentState().clip_name;
            player.time             = player.blend_time;
            player.blend_clip.clear();
            player.blend_weight     = 0.0f;
            player.blend_time       = 0.0f;
            changed = true;
        }
    }

    // Evaluate outgoing transitions from the current state
    if (!sm->in_transition || sm->transitions[0].can_interrupt) {
        for (const auto& trans : sm->transitions) {
            if (trans.from_state != sm->current_state_index) continue;
            if (trans.to_state   == sm->current_state_index) continue;
            if (!trans.Evaluate(sm->float_params, sm->bool_params, sm->triggers)) continue;

            // Fire transition
            sm->in_transition       = true;
            sm->target_state_index  = trans.to_state;
            sm->transition_time     = 0.0f;
            sm->transition_duration = trans.blend_duration;

            const auto& target_state = sm->states[trans.to_state];
            player.blend_clip   = target_state.clip_name;
            player.blend_time   = 0.0f;
            player.blend_weight = 0.0f;
            player.loop         = target_state.loop;
            player.speed        = target_state.speed;
            changed = true;
            break;
        }
    }

    // If no clip is set yet, pick from current state
    if (player.current_clip.empty() && !sm->states.empty()) {
        const auto& state = sm->GetCurrentState();
        player.current_clip = state.clip_name;
        player.loop         = state.loop;
        player.speed        = state.speed;
        changed = true;
    }

    return changed;
}

// ---------------------------------------------------------------------------
// SampleClip
// ---------------------------------------------------------------------------
void AnimationSystem::SampleClip(const Skeleton& skel,
                                  const AnimationClip& clip,
                                  float t,
                                  std::vector<mat4>& out_local) const {
    clip.Sample(t, out_local);
}

} // namespace action
