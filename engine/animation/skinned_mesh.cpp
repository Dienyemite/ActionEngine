#include "skinned_mesh.h"
#include "core/math/math.h"

namespace action {

// ---------------------------------------------------------------------------
// Transform a single position by a skinned matrix blend
// ---------------------------------------------------------------------------
static vec3 TransformPos(const vec3& p, const mat4& m) {
    return {
        m.m[0][0]*p.x + m.m[1][0]*p.y + m.m[2][0]*p.z + m.m[3][0],
        m.m[0][1]*p.x + m.m[1][1]*p.y + m.m[2][1]*p.z + m.m[3][1],
        m.m[0][2]*p.x + m.m[1][2]*p.y + m.m[2][2]*p.z + m.m[3][2],
    };
}

// Transform a direction vector (no translation, no w-divide)
static vec3 TransformDir(const vec3& d, const mat4& m) {
    return {
        m.m[0][0]*d.x + m.m[1][0]*d.y + m.m[2][0]*d.z,
        m.m[0][1]*d.x + m.m[1][1]*d.y + m.m[2][1]*d.z,
        m.m[0][2]*d.x + m.m[1][2]*d.y + m.m[2][2]*d.z,
    };
}

static vec3 AddVec3(const vec3& a, const vec3& b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static vec3 ScaleVec3(const vec3& a, float s)     { return {a.x*s,   a.y*s,   a.z*s}; }

void CPUSkinMesh(const SkinnedMeshData& skinned,
                 const std::vector<mat4>& skinning_matrices,
                 std::vector<SkinnedVertex>& out_vertices) {
    const u32 vert_count = (u32)skinned.bind_vertices.size();
    out_vertices.resize(vert_count);

    for (u32 vi = 0; vi < vert_count; ++vi) {
        const SkinnedVertex& src = skinned.bind_vertices[vi];
        SkinnedVertex& dst = out_vertices[vi];

        // Copy non-positional data
        dst = src;

        vec3 blended_pos = {0, 0, 0};
        vec3 blended_nrm = {0, 0, 0};

        for (int b = 0; b < 4; ++b) {
            float w = src.bone_weights[b];
            if (w < 1e-7f) continue;

            const u32 bi = src.bone_indices[b];
            if (bi >= skinning_matrices.size()) continue;
            const mat4& m = skinning_matrices[bi];

            blended_pos = AddVec3(blended_pos, ScaleVec3(TransformPos(src.position, m), w));
            blended_nrm = AddVec3(blended_nrm, ScaleVec3(TransformDir(src.normal,   m), w));
        }

        dst.position = blended_pos;

        // Re-normalise normal (blending can shrink it)
        float len = std::sqrt(blended_nrm.x*blended_nrm.x +
                               blended_nrm.y*blended_nrm.y +
                               blended_nrm.z*blended_nrm.z);
        if (len > 0.0001f) {
            float inv = 1.0f / len;
            dst.normal = {blended_nrm.x*inv, blended_nrm.y*inv, blended_nrm.z*inv};
        } else {
            dst.normal = {0, 1, 0};
        }
    }
}

} // namespace action
