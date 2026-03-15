#pragma once

/*
 * Skeleton — bone hierarchy for skeletal animation.
 *
 * Stores the rest-pose bone hierarchy extracted from a skinned mesh.
 * At runtime the AnimationSystem evaluates an AnimationClip against this
 * skeleton to produce per-bone local transforms, then multiplies up to
 * world space before CPU-skinning the mesh vertices.
 *
 * TODO(animation): Move skinning to a vertex-shader pass once the
 * pipeline supports per-instance bone SSBO uploads.
 */

#include "core/types.h"
#include <string>
#include <vector>

namespace action {

// SkeletonHandle — typed handle for use with AnimationLibrary
struct SkeletonTag {};
using SkeletonHandle = Handle<SkeletonTag>;

// -------------------------------------------------------------------------
// Bone
// -------------------------------------------------------------------------
struct Bone {
    std::string name;

    // Index of parent bone in Skeleton::bones; -1 for root bones.
    i32 parent_index = -1;

    // Rest-pose local transform (relative to parent), read from the source
    // file's node/joint hierarchy.
    mat4 local_rest_transform;

    // Inverse of the bone's world-space rest pose (a.k.a. "offset matrix" or
    // "inverse bind pose").  Multiplying a world-space vertex position by this
    // brings it into bone-local space; multiplied by the current world-pose
    // produces the final skinned position.
    mat4 inv_bind_pose;
};

// -------------------------------------------------------------------------
// Skeleton
// -------------------------------------------------------------------------
struct Skeleton {
    std::string name;
    std::vector<Bone> bones;

    // Return the index of a bone by name, or -1 if not found.
    i32 FindBone(std::string_view bone_name) const {
        for (i32 i = 0; i < static_cast<i32>(bones.size()); ++i) {
            if (bones[i].name == bone_name) return i;
        }
        return -1;
    }

    bool is_valid() const { return !bones.empty(); }

    // Compute the world-space transform of bone[index] given a flat array of
    // evaluated local transforms (one per bone, same order as bones[]).
    // out_world must be pre-sized to bones.size().
    void ComputeWorldTransforms(const std::vector<mat4>& local_transforms,
                                 std::vector<mat4>& out_world) const;

    // Compute the final skinning matrices:
    //   skinning[i] = world[i] * inv_bind_pose[i]
    // These are the matrices uploaded to the vertex shader / CPU skinner.
    void ComputeSkinningMatrices(const std::vector<mat4>& local_transforms,
                                  std::vector<mat4>& out_skinning) const;
};

} // namespace action
