#include "blend_tree.h"
#include "skeleton.h"
#include "skinned_mesh.h"
#include "core/math/math.h"
#include "core/logging.h"
#include <cmath>
#include <numeric>

namespace action {

// ---------------------------------------------------------------------------
// BlendTreeSystem::BlendPoses
// ---------------------------------------------------------------------------
void BlendTreeSystem::BlendPoses(std::vector<mat4>& out_a,
                                  const std::vector<mat4>& b,
                                  float t,
                                  u32 bone_count)
{
    for (u32 i = 0; i < bone_count; ++i) {
        float*       fa = &out_a[i].columns[0].x;
        const float* fb = &b[i].columns[0].x;
        for (int e = 0; e < 16; ++e)
            fa[e] = Lerp(fa[e], fb[e], t);
    }
}

// ---------------------------------------------------------------------------
// BlendTreeSystem::EvaluateNode
// ---------------------------------------------------------------------------
bool BlendTreeSystem::EvaluateNode(u32 node_idx,
                                    float dt,
                                    BlendTree& tree,
                                    const Skeleton& skel,
                                    std::vector<mat4>& out_local) const
{
    if (node_idx >= (u32)tree.nodes.size()) return false;

    BlendNode& node      = tree.nodes[node_idx];
    const u32  bone_count = (u32)skel.bones.size();

    // Helper: fill out_local with skeleton rest pose.
    auto fill_rest = [&](std::vector<mat4>& buf) {
        buf.resize(bone_count, mat4::identity());
        for (u32 i = 0; i < bone_count; ++i)
            buf[i] = skel.bones[i].local_rest_transform;
    };

    switch (node.type) {

    // -----------------------------------------------------------------------
    case BlendNodeType::Clip:
    {
        const AnimationClip* clip = m_library->GetClip(node.clip_name);
        if (!clip || !clip->is_valid()) return false;

        node.time += dt * node.speed;
        node.time  = clip->WrapTime(node.time);

        fill_rest(out_local);
        clip->Sample(node.time, out_local);
        return true;
    }

    // -----------------------------------------------------------------------
    case BlendNodeType::Lerp1D:
    {
        if (node.children.size() < 2) return false;

        const float param = tree.GetFloat(node.param_name);
        const float range = node.param_high - node.param_low;
        float t;
        if (range <= EPSILON)
            t = (param >= node.param_high) ? 1.f : 0.f;
        else
            t = Clamp((param - node.param_low) / range, 0.f, 1.f);

        std::vector<mat4> pose_a, pose_b;
        fill_rest(pose_a);
        fill_rest(pose_b);

        const bool ok_a = EvaluateNode(node.children[0], dt, tree, skel, pose_a);
        const bool ok_b = EvaluateNode(node.children[1], dt, tree, skel, pose_b);

        if (!ok_a && !ok_b) return false;
        if (!ok_a) { out_local = std::move(pose_b); return true; }
        if (!ok_b) { out_local = std::move(pose_a); return true; }

        out_local = std::move(pose_a);
        BlendPoses(out_local, pose_b, t, bone_count);
        return true;
    }

    // -----------------------------------------------------------------------
    case BlendNodeType::Lerp2D:
    {
        const u32 n = (u32)node.samples_2d.size();
        if (n == 0) return false;

        const float px = tree.GetFloat(node.param_x);
        const float py = tree.GetFloat(node.param_y);

        // Inverse-distance weighting in the 2D parameter space.
        std::vector<float> weights(n);
        float sum = 0.f;
        for (u32 i = 0; i < n; ++i) {
            float dx = px - node.samples_2d[i].px;
            float dy = py - node.samples_2d[i].py;
            float d2 = dx * dx + dy * dy;
            weights[i] = (d2 < EPSILON * EPSILON) ? 1e9f : 1.f / d2;
            sum += weights[i];
        }
        if (sum < EPSILON) {
            float uniform = 1.f / (float)n;
            for (auto& w : weights) w = uniform;
        } else {
            for (auto& w : weights) w /= sum;
        }

        // Advance shared playback time and accumulate weighted poses.
        node.time_2d += dt;

        fill_rest(out_local);
        for (u32 i = 0; i < bone_count; ++i) {
            float* fe = &out_local[i].columns[0].x;
            for (int e = 0; e < 16; ++e) fe[e] = 0.f;
        }

        for (u32 si = 0; si < n; ++si) {
            if (weights[si] < EPSILON) continue;
            const AnimationClip* clip =
                m_library->GetClip(node.samples_2d[si].clip_name);
            if (!clip || !clip->is_valid()) continue;

            std::vector<mat4> sample_pose;
            fill_rest(sample_pose);
            clip->Sample(clip->WrapTime(node.time_2d), sample_pose);

            const float w = weights[si];
            for (u32 i = 0; i < bone_count; ++i) {
                float*       fd = &out_local[i].columns[0].x;
                const float* fs = &sample_pose[i].columns[0].x;
                for (int e = 0; e < 16; ++e)
                    fd[e] += fs[e] * w;
            }
        }
        return true;
    }

    // -----------------------------------------------------------------------
    case BlendNodeType::Additive:
    {
        if (node.children.size() < 2) return false;

        // Evaluate base pose.
        fill_rest(out_local);
        if (!EvaluateNode(node.children[0], dt, tree, skel, out_local))
            return false;

        // Evaluate additive layer.
        std::vector<mat4> additive;
        fill_rest(additive);
        if (!EvaluateNode(node.children[1], dt, tree, skel, additive))
            return true;  // Return base unchanged.

        // Apply delta: base + (additive - rest) * weight.
        const float w = node.additive_weight;
        for (u32 i = 0; i < bone_count; ++i) {
            const float* frest = &skel.bones[i].local_rest_transform.columns[0].x;
            const float* fadd  = &additive[i].columns[0].x;
            float*       fout  = &out_local[i].columns[0].x;
            for (int e = 0; e < 16; ++e)
                fout[e] += (fadd[e] - frest[e]) * w;
        }
        return true;
    }

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// BlendTreeSystem::Update
// ---------------------------------------------------------------------------
void BlendTreeSystem::Update(float dt) {
    m_ecs->ForEach<BlendTreeComponent, AnimationPlayerComponent>(
        [&](Entity /*entity*/,
            BlendTreeComponent& btc,
            AnimationPlayerComponent& player)
    {
        if (btc.skeleton_name.empty() || btc.tree.nodes.empty()) return;

        const Skeleton* skel = m_library->GetSkeleton(btc.skeleton_name);
        if (!skel || !skel->is_valid()) return;

        const u32 bone_count = (u32)skel->bones.size();
        std::vector<mat4> local_xforms(bone_count, mat4::identity());
        for (u32 i = 0; i < bone_count; ++i)
            local_xforms[i] = skel->bones[i].local_rest_transform;

        if (!EvaluateNode(btc.tree.root, dt, btc.tree, *skel, local_xforms))
            return;

        // Write into AnimationPlayerComponent so IKSystem runs on these poses.
        player.skeleton_name    = btc.skeleton_name;
        player.local_transforms = local_xforms;

        player.skinning_matrices.resize(bone_count);
        skel->ComputeSkinningMatrices(local_xforms, player.skinning_matrices);

        // CPU-skin if a mesh is bound.
        SkinnedMeshData* skinned = m_library->GetSkinnedMesh(btc.skeleton_name);
        if (skinned && skinned->is_valid())
            CPUSkinMesh(*skinned, player.skinning_matrices,
                         skinned->posed_vertices);
    });
}

} // namespace action
