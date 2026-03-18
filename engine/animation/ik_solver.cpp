#include "ik_solver.h"
#include "skeleton.h"
#include "skinned_mesh.h"
#include "core/math/math.h"
#include "core/logging.h"
#include <cmath>

namespace action {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static inline vec3 PosOf(const mat4& m) {
    return { m.columns[3].x, m.columns[3].y, m.columns[3].z };
}

// Apply a world-space delta rotation around a pivot point to a world matrix.
//   result = translate(pivot) * rotate(delta) * translate(-pivot) * world
// This changes the orientation encoded in 'world' while keeping the bone's
// own origin (= pivot) in place.
static mat4 RotateAroundPoint(const mat4& delta_rot,
                               const vec3& pivot,
                               const mat4& world) {
    mat4 T_p   = mat4::translate(pivot);
    mat4 T_neg = mat4::translate({ -pivot.x, -pivot.y, -pivot.z });
    return T_p * delta_rot * T_neg * world;
}

// Element-wise linear blend between two mat4s at weight t in [0,1].
static void BlendMat4(mat4& dst, const mat4& a, const mat4& b, float t) {
    if (t >= 1.f) { dst = b; return; }
    if (t <= 0.f) { dst = a; return; }
    for (int c = 0; c < 4; ++c) {
        const float* fa = &a.columns[c].x;
        const float* fb = &b.columns[c].x;
        float*       fd = &dst.columns[c].x;
        for (int r = 0; r < 4; ++r)
            fd[r] = Lerp(fa[r], fb[r], t);
    }
}

// ---------------------------------------------------------------------------
// IKSystem::RotationBetween
// ---------------------------------------------------------------------------
quat IKSystem::RotationBetween(const vec3& from, const vec3& to) {
    const float d = dot(from, to);
    if (d > 0.9999f) return quat::identity();
    if (d < -0.9999f) {
        // 180° rotation — pick any perpendicular axis.
        vec3 perp = (std::abs(from.x) < 0.9f)
                  ? cross(from, { 1.f, 0.f, 0.f })
                  : cross(from, { 0.f, 1.f, 0.f });
        return quat::from_axis_angle(perp.normalized(), PI);
    }
    return quat::from_axis_angle(cross(from, to).normalized(),
                                  std::acos(Clamp(d, -1.f, 1.f)));
}

// ---------------------------------------------------------------------------
// IKSystem::Update
// ---------------------------------------------------------------------------
void IKSystem::Update(float /*dt*/) {
    m_ecs->ForEach<AnimationPlayerComponent, IKComponent>(
        [&](Entity /*entity*/,
            AnimationPlayerComponent& player,
            IKComponent& ik)
    {
        if (player.local_transforms.empty() || player.skeleton_name.empty()) return;

        const Skeleton* skel = m_library->GetSkeleton(player.skeleton_name);
        if (!skel || !skel->is_valid()) return;

        // Resolve bone name → index on first use.
        for (IKChain& chain : ik.chains) {
            if (!chain.enabled || chain.weight <= 0.f) continue;
            if (chain.root_idx < 0 && !chain.root_bone.empty())
                chain.root_idx = skel->FindBone(chain.root_bone);
            if (chain.type == IKType::TwoBone) {
                if (chain.mid_idx < 0 && !chain.mid_bone.empty())
                    chain.mid_idx = skel->FindBone(chain.mid_bone);
                if (chain.tip_idx < 0 && !chain.tip_bone.empty())
                    chain.tip_idx = skel->FindBone(chain.tip_bone);
            }
        }

        bool any_solved = false;
        for (IKChain& chain : ik.chains) {
            if (!chain.enabled || chain.weight <= 0.f || chain.root_idx < 0) continue;
            if (chain.type == IKType::TwoBone) {
                if (chain.mid_idx < 0 || chain.tip_idx < 0) continue;
                SolveTwoBone(chain, *skel, player.local_transforms);
            } else {
                SolveLookAt(chain, *skel, player.local_transforms);
            }
            any_solved = true;
        }

        if (any_solved) {
            // Recompute skinning matrices from the corrected local transforms.
            const u32 n = (u32)skel->bones.size();
            player.skinning_matrices.resize(n);
            skel->ComputeSkinningMatrices(player.local_transforms,
                                           player.skinning_matrices);

            // Re-skin the CPU mesh if one is attached.
            SkinnedMeshData* skinned =
                m_library->GetSkinnedMesh(player.skeleton_name);
            if (skinned && skinned->is_valid())
                CPUSkinMesh(*skinned, player.skinning_matrices,
                             skinned->posed_vertices);
        }
    });
}

// ---------------------------------------------------------------------------
// IKSystem::SolveTwoBone
//
// Analytical three-bone chain IK.
//
//   A (root) ──── B (mid) ──── C (tip)  →  place C at target T
//
// The algorithm:
//   1. Use the law of cosines to find the angle at A so the chain reaches T.
//   2. Compute B_new using the bend direction (pole vector).
//   3. Rotate A to aim its tail at B_new.
//   4. Recompute world transforms and rotate B to aim its tail at T.
// ---------------------------------------------------------------------------
void IKSystem::SolveTwoBone(IKChain& chain,
                              const Skeleton& skel,
                              std::vector<mat4>& local_xforms) const
{
    const i32 ri = chain.root_idx;
    const i32 mi = chain.mid_idx;
    const i32 ti = chain.tip_idx;

    // Full world transform pass.
    std::vector<mat4> world;
    skel.ComputeWorldTransforms(local_xforms, world);

    const vec3 A = PosOf(world[ri]);
    const vec3 B = PosOf(world[mi]);
    const vec3 C = PosOf(world[ti]);
    const vec3 T = chain.target_world;

    // Bone segment lengths.
    const float la = distance(B, A);
    const float lb = distance(C, B);
    if (la < EPSILON || lb < EPSILON) return;

    // Clamp reach.
    const float max_reach = la + lb - EPSILON;
    const float dist_AT   = distance(T, A);
    const float d         = std::min(dist_AT, max_reach);

    // Primary direction (root → target).
    const vec3 dir_AT = (dist_AT > EPSILON) ? normalize(T - A) : normalize(B - A);

    // Bend direction (pole vector projected onto the plane perpendicular to dir_AT).
    vec3 bend_dir;
    {
        vec3 pv   = chain.pole_world - A;
        float prj = dot(pv, dir_AT);
        bend_dir  = pv - dir_AT * prj;
        if (bend_dir.length_sq() < EPSILON * EPSILON) {
            // Fallback: current mid-bone direction projected perp to dir_AT.
            vec3 cur = B - A;
            bend_dir = cur - dir_AT * dot(cur, dir_AT);
        }
        if (bend_dir.length_sq() < EPSILON * EPSILON)
            bend_dir = (std::abs(dir_AT.y) < 0.9f) ? vec3{0,1,0} : vec3{1,0,0};
        bend_dir = bend_dir.normalized();
    }

    // Law of cosines: angle at A so that B is at distance la from A and lb from T.
    const float cos_A = Clamp((la*la + d*d - lb*lb) / (2.f * la * d), -1.f, 1.f);
    const float sin_A = std::sqrt(std::max(0.f, 1.f - cos_A * cos_A));

    // Desired mid-bone world position.
    const vec3 B_new = A + dir_AT * (la * cos_A) + bend_dir * (la * sin_A);

    // ---- Step 1: Rotate root bone A so its tail tracks B_new ----
    {
        const mat4 orig_local = local_xforms[ri];

        const vec3 from_dir = normalize(B     - A);
        const vec3 to_dir   = normalize(B_new - A);
        const mat4 delta    = mat4::rotate(RotationBetween(from_dir, to_dir));

        const i32   par_idx  = skel.bones[ri].parent_index;
        const mat4& par_w    = (par_idx >= 0) ? world[par_idx] : mat4::identity();
        const mat4  par_w_inv = par_w.inverse();

        const mat4 new_world_ri = RotateAroundPoint(delta, A, world[ri]);
        const mat4 new_local    = par_w_inv * new_world_ri;

        mat4 blended = orig_local;
        BlendMat4(blended, orig_local, new_local, chain.weight);
        local_xforms[ri] = blended;
    }

    // ---- Recompute world transforms after root change ----
    skel.ComputeWorldTransforms(local_xforms, world);

    // ---- Step 2: Rotate mid bone B so its tail tracks T ----
    {
        const mat4 orig_local = local_xforms[mi];

        const vec3 B_cur  = PosOf(world[mi]);
        const vec3 C_cur  = PosOf(world[ti]);

        const vec3 from_dir = normalize(C_cur - B_cur);
        const vec3 to_dir   = normalize(T     - B_cur);
        const mat4 delta    = mat4::rotate(RotationBetween(from_dir, to_dir));

        const i32   par_idx  = skel.bones[mi].parent_index;
        const mat4& par_w    = (par_idx >= 0) ? world[par_idx] : mat4::identity();
        const mat4  par_w_inv = par_w.inverse();

        const mat4 new_world_mi = RotateAroundPoint(delta, B_cur, world[mi]);
        const mat4 new_local    = par_w_inv * new_world_mi;

        mat4 blended = orig_local;
        BlendMat4(blended, orig_local, new_local, chain.weight);
        local_xforms[mi] = blended;
    }
}

// ---------------------------------------------------------------------------
// IKSystem::SolveLookAt
//
// Rotates one bone so that its local forward_axis faces target_world.
// Applies the correction on top of the animated pose at the given weight.
// ---------------------------------------------------------------------------
void IKSystem::SolveLookAt(IKChain& chain,
                             const Skeleton& skel,
                             std::vector<mat4>& local_xforms) const
{
    const i32 bi = chain.root_idx;

    std::vector<mat4> world;
    skel.ComputeWorldTransforms(local_xforms, world);

    const vec3 bone_pos = PosOf(world[bi]);
    const vec3 dir      = chain.target_world - bone_pos;
    if (dir.length_sq() < EPSILON * EPSILON) return;
    const vec3 dir_n = dir.normalized();

    // Transform the bone's local forward axis to world space (rotation only).
    mat4 world_rot = world[bi];
    world_rot.columns[3] = { 0.f, 0.f, 0.f, 1.f };
    const vec4 fw4 = world_rot * vec4{ chain.forward_axis.x,
                                       chain.forward_axis.y,
                                       chain.forward_axis.z, 0.f };
    const vec3 current_fwd = vec3{ fw4.x, fw4.y, fw4.z }.normalized();

    const mat4 delta     = mat4::rotate(RotationBetween(current_fwd, dir_n));
    const i32  par_idx   = skel.bones[bi].parent_index;
    const mat4& par_w    = (par_idx >= 0) ? world[par_idx] : mat4::identity();
    const mat4  par_w_inv = par_w.inverse();

    const mat4 new_world = RotateAroundPoint(delta, bone_pos, world[bi]);
    const mat4 new_local = par_w_inv * new_world;

    mat4 blended = local_xforms[bi];
    BlendMat4(blended, local_xforms[bi], new_local, chain.weight);
    local_xforms[bi] = blended;
}

} // namespace action
