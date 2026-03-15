#include "animation_clip.h"
#include "skeleton.h"
#include "core/math/math.h"
#include <cmath>
#include <algorithm>

namespace action {

// ---------------------------------------------------------------------------
// Helpers: find bracketing keyframes and lerp/slerp
// ---------------------------------------------------------------------------

static vec3 LerpVec3(const vec3& a, const vec3& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

static quat SlerpQuat(const quat& a, const quat& b, float t) {
    // Ensure the shortest path is taken
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    quat b2 = (dot < 0.0f) ? quat{-b.x,-b.y,-b.z,-b.w} : b;
    dot = std::abs(dot);

    // Fall back to lerp when the quats are nearly identical
    if (dot > 0.9995f) {
        quat result{
            a.x + (b2.x - a.x) * t,
            a.y + (b2.y - a.y) * t,
            a.z + (b2.z - a.z) * t,
            a.w + (b2.w - a.w) * t
        };
        float len = std::sqrt(result.x*result.x + result.y*result.y +
                               result.z*result.z + result.w*result.w);
        if (len > 0.0001f) {
            float inv = 1.0f / len;
            result.x *= inv; result.y *= inv; result.z *= inv; result.w *= inv;
        }
        return result;
    }

    float angle    = std::acos(dot);
    float sin_a    = std::sin(angle);
    float factor_a = std::sin((1.0f - t) * angle) / sin_a;
    float factor_b = std::sin(t * angle) / sin_a;

    return {
        factor_a * a.x + factor_b * b2.x,
        factor_a * a.y + factor_b * b2.y,
        factor_a * a.z + factor_b * b2.z,
        factor_a * a.w + factor_b * b2.w,
    };
}

// Binary search for the two keyframes that surround 'time'.
// Returns [0, size-1].
template<typename K>
static void FindBracket(const std::vector<K>& keys, float time, u32& lo, u32& hi) {
    lo = 0;
    hi = (u32)keys.size() - 1;

    if (time <= keys.front().time) { hi = lo = 0; return; }
    if (time >= keys.back().time)  { lo = hi = hi; return; }

    u32 mid = 0;
    while (lo + 1 < hi) {
        mid = (lo + hi) / 2;
        if (keys[mid].time <= time) lo = mid;
        else                        hi = mid;
    }
}

static float InvLerp(float a, float b, float v) {
    float span = b - a;
    return (span > 1e-7f) ? (v - a) / span : 0.0f;
}

// ---------------------------------------------------------------------------
// BoneChannel::Sample*
// ---------------------------------------------------------------------------

vec3 BoneChannel::SamplePosition(float time) const {
    if (position_keys.empty()) return {0, 0, 0};
    if (position_keys.size() == 1) return position_keys[0].value;
    u32 lo, hi;
    FindBracket(position_keys, time, lo, hi);
    if (lo == hi) return position_keys[lo].value;
    float t = InvLerp(position_keys[lo].time, position_keys[hi].time, time);
    return LerpVec3(position_keys[lo].value, position_keys[hi].value, t);
}

quat BoneChannel::SampleRotation(float time) const {
    if (rotation_keys.empty()) return quat::identity();
    if (rotation_keys.size() == 1) return rotation_keys[0].value;
    u32 lo, hi;
    FindBracket(rotation_keys, time, lo, hi);
    if (lo == hi) return rotation_keys[lo].value;
    float t = InvLerp(rotation_keys[lo].time, rotation_keys[hi].time, time);
    return SlerpQuat(rotation_keys[lo].value, rotation_keys[hi].value, t);
}

vec3 BoneChannel::SampleScale(float time) const {
    if (scale_keys.empty()) return {1, 1, 1};
    if (scale_keys.size() == 1) return scale_keys[0].value;
    u32 lo, hi;
    FindBracket(scale_keys, time, lo, hi);
    if (lo == hi) return scale_keys[lo].value;
    float t = InvLerp(scale_keys[lo].time, scale_keys[hi].time, time);
    return LerpVec3(scale_keys[lo].value, scale_keys[hi].value, t);
}

// ---------------------------------------------------------------------------
// AnimationClip::Sample
// ---------------------------------------------------------------------------
void AnimationClip::Sample(float t, std::vector<mat4>& out_local_transforms) const {
    // Caller pre-fills out_local_transforms with rest-pose matrices.
    // We overwrite only the bones referenced by our channels.
    for (const auto& chan : channels) {
        if (chan.bone_index < 0 || chan.bone_index >= (i32)out_local_transforms.size()) continue;

        vec3 pos   = chan.SamplePosition(t);
        quat rot   = chan.SampleRotation(t);
        vec3 scale = chan.SampleScale(t);

        out_local_transforms[chan.bone_index] =
            mat4::translate(pos) * mat4::rotate(rot) * mat4::scale(scale);
    }
}

// ---------------------------------------------------------------------------
// Skeleton::ComputeWorldTransforms
// ---------------------------------------------------------------------------
void Skeleton::ComputeWorldTransforms(const std::vector<mat4>& local,
                                       std::vector<mat4>& out_world) const {
    const u32 n = (u32)bones.size();
    out_world.resize(n, mat4::identity());

    for (u32 i = 0; i < n; ++i) {
        if (bones[i].parent_index < 0) {
            out_world[i] = local[i];
        } else {
            out_world[i] = out_world[bones[i].parent_index] * local[i];
        }
    }
}

// ---------------------------------------------------------------------------
// Skeleton::ComputeSkinningMatrices
// ---------------------------------------------------------------------------
void Skeleton::ComputeSkinningMatrices(const std::vector<mat4>& local,
                                        std::vector<mat4>& out_skinning) const {
    const u32 n = (u32)bones.size();
    std::vector<mat4> world;
    ComputeWorldTransforms(local, world);

    out_skinning.resize(n);
    for (u32 i = 0; i < n; ++i) {
        out_skinning[i] = world[i] * bones[i].inv_bind_pose;
    }
}

} // namespace action
