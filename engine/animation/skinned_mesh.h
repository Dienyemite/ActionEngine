#pragma once

/*
 * Skinned mesh data and CPU-skinning helper.
 *
 * SkinnedVertex extends the base geometry with up to 4 bone influences.
 * CPU skinning is applied every frame inside AnimationSystem::Update() for
 * entities that have both AnimationPlayerComponent and a skinned mesh in
 * the AssetManager.
 *
 * TODO(animation): Replace CPU skinning with a Vulkan compute dispatch once
 * the per-instance bone matrix SSBO upload path is in place.
 */

#include "core/types.h"
#include <vector>

namespace action {

// Vertex format for skinned meshes — 4-bone influence limit.
// Bone indices reference Skeleton::bones[].
// The four weights are normalised (sum == 1).
struct SkinnedVertex {
    vec3  position;
    vec3  normal;
    vec2  uv;
    vec3  tangent;

    u8    bone_indices[4] = {0, 0, 0, 0};
    float bone_weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

// Per-vertex bone influence before packing into SkinnedVertex.
// Collecting all influences first allows them to be sorted and normalised.
struct VertexInfluence {
    u32   bone_index = 0;
    float weight     = 0.0f;
};

// SkinnedMeshData — all the CPU-side data needed to skin a mesh.
// Stored alongside MeshData in the AssetManager (via name-keyed map in
// AnimationLibrary rather than inside MeshData, to keep the asset system
// decoupled from animation).
struct SkinnedMeshData {
    std::string mesh_name;                  // Matches MeshData::name
    std::string skeleton_name;              // Matches Skeleton::name in AnimationLibrary

    // Skinned vertex buffer (positional data in bind pose; bones modify it).
    std::vector<SkinnedVertex> bind_vertices;

    // Scratch buffer reused each frame: filled by CPUSkinMesh().
    // Sized to bind_vertices.size() on first call.
    mutable std::vector<SkinnedVertex> posed_vertices;

    bool is_valid() const { return !bind_vertices.empty() && !skeleton_name.empty(); }
};

// ---------------------------------------------------------------------------
// CPU Skinning
//
// Transform bind_vertices by the skinning matrices (one per bone) and write
// the result into out_vertices.  out_vertices is resized if necessary.
//
// skinning_matrices[i] = world_pose[i] * inv_bind_pose[i]
//   (produced by Skeleton::ComputeSkinningMatrices)
// ---------------------------------------------------------------------------
void CPUSkinMesh(const SkinnedMeshData& skinned,
                 const std::vector<mat4>& skinning_matrices,
                 std::vector<SkinnedVertex>& out_vertices);

} // namespace action
