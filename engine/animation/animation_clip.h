#pragma once

/*
 * AnimationClip — time-sampled bone transforms.
 *
 * A clip stores one BoneChannel per animated bone.  Each channel holds sorted
 * keyframe arrays for position, rotation, and scale.  When sampled at a given
 * time, the clip linearly interpolates (LERP / SLERP) between the surrounding
 * keyframes and writes one mat4 per bone into the caller-supplied buffer.
 *
 * Bones that are not referenced by any channel keep their rest-pose transform
 * (already in the caller's buffer from Skeleton::local_rest_transform).
 */

#include "core/types.h"
#include "core/math/math.h"
#include <string>
#include <vector>

namespace action {

struct AnimationTag {};
using AnimationClipHandle = Handle<AnimationTag>;

// ---------------------------------------------------------------------------
// Keyframe types
// ---------------------------------------------------------------------------
struct AnimKeyPos   { float time; vec3 value; };
struct AnimKeyRot   { float time; quat value; };
struct AnimKeyScale { float time; vec3 value; };

// ---------------------------------------------------------------------------
// BoneChannel — keyframe curves for a single bone
// ---------------------------------------------------------------------------
struct BoneChannel {
    i32 bone_index = -1;    // Index into Skeleton::bones

    std::vector<AnimKeyPos>   position_keys;
    std::vector<AnimKeyRot>   rotation_keys;
    std::vector<AnimKeyScale> scale_keys;

    // Sample position at 'time' (seconds) via linear interpolation.
    vec3 SamplePosition(float time) const;

    // Sample rotation at 'time' (seconds) via spherical linear interpolation.
    quat SampleRotation(float time) const;

    // Sample scale at 'time' (seconds) via linear interpolation.
    vec3 SampleScale(float time) const;
};

// ---------------------------------------------------------------------------
// AnimationClip
// ---------------------------------------------------------------------------
struct AnimationClip {
    std::string name;
    float duration   = 0.0f;    // Total length in seconds
    bool  looping    = true;

    // One entry per animated bone (NOT one per skeleton bone).
    std::vector<BoneChannel> channels;

    // Sample all channels at time 't', writing TRS matrices into
    // out_local_transforms.  The buffer must be pre-sized to
    // Skeleton::bones.size().  Bones not referenced by any channel are
    // untouched (caller should pre-fill with rest-pose transforms).
    void Sample(float t, std::vector<mat4>& out_local_transforms) const;

    // Wrap 't' into [0, duration) respecting looping.
    float WrapTime(float t) const {
        if (duration <= 0.0f) return 0.0f;
        if (!looping) return (t < duration) ? t : duration;
        return std::fmod(t, duration);
    }

    bool is_valid() const { return !channels.empty() && duration > 0.0f; }
};

} // namespace action
