#include "frustum_culling.h"
#include "core/math/math.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace action {

void FrustumCuller::SetFrustum(const Frustum& frustum, const vec3& camera_pos) {
    m_frustum = frustum;
    m_camera_pos = camera_pos;
}

bool FrustumCuller::IsVisible(const AABB& bounds) const {
    return m_frustum.intersects(bounds);
}

bool FrustumCuller::IsVisible(const Sphere& bounds) const {
    return m_frustum.intersects(bounds);
}

bool FrustumCuller::IsVisible(const vec3& position, float radius) const {
    return m_frustum.intersects(Sphere{position, radius});
}

void FrustumCuller::CullAABBs(const AABB* bounds, u32 count, 
                               std::vector<CullResult>& results) const {
    results.clear();
    // Reserve capacity to avoid repeated reallocations
    // Assume ~50% visibility as heuristic
    if (results.capacity() < count / 2) {
        results.reserve(count / 2);
    }
    
    for (u32 i = 0; i < count; ++i) {
        if (m_frustum.intersects(bounds[i])) {
            results.push_back({
                i,
                true,
                distance_sq(bounds[i].center(), m_camera_pos)
            });
        }
    }
}

void FrustumCuller::CullSpheres(const Sphere* bounds, u32 count,
                                 std::vector<CullResult>& results) const {
    results.clear();
    // Reserve capacity to avoid repeated reallocations
    if (results.capacity() < count / 2) {
        results.reserve(count / 2);
    }
    
    for (u32 i = 0; i < count; ++i) {
        if (m_frustum.intersects(bounds[i])) {
            results.push_back({
                i,
                true,
                distance_sq(bounds[i].center, m_camera_pos)
            });
        }
    }
}

void FrustumCuller::CullWithDistance(const AABB* bounds, u32 count,
                                      float max_distance,
                                      std::vector<CullResult>& results) const {
    results.clear();
    results.reserve(count);
    
    float max_dist_sq = max_distance * max_distance;
    
    for (u32 i = 0; i < count; ++i) {
        float dist_sq = distance_sq(bounds[i].center(), m_camera_pos);
        
        // Early distance rejection
        if (dist_sq > max_dist_sq) {
            continue;
        }
        
        // Frustum test
        if (m_frustum.intersects(bounds[i])) {
            CullResult result;
            result.object_index = i;
            result.visible = true;
            result.distance_sq = dist_sq;
            results.push_back(result);
        }
    }
}

// Occlusion Culler

bool OcclusionCuller::Initialize(u32 width, u32 height) {
    m_width = width;
    m_height = height;
    m_depth_buffer.resize(width * height, 1.0f);
    return true;
}

void OcclusionCuller::Shutdown() {
    m_depth_buffer.clear();
    m_width = 0;
    m_height = 0;
}

void OcclusionCuller::RasterizeOccluder(const vec3* vertices, u32 vertex_count,
                                         const u32* indices, u32 index_count,
                                         const mat4& mvp) {
    if (!vertices || !indices || vertex_count == 0 || index_count == 0) return;
    if (m_width == 0 || m_height == 0) return;

    const float hw = static_cast<float>(m_width)  * 0.5f;
    const float hh = static_cast<float>(m_height) * 0.5f;

    // Project all vertices to screen space
    std::vector<vec4> ndc(vertex_count);
    for (u32 i = 0; i < vertex_count; ++i) {
        ndc[i] = mvp * vec4(vertices[i], 1.0f);
    }

    // Rasterize each triangle
    for (u32 tri = 0; tri + 2 < index_count; tri += 3) {
        const vec4& v0 = ndc[indices[tri]];
        const vec4& v1 = ndc[indices[tri + 1]];
        const vec4& v2 = ndc[indices[tri + 2]];

        // Clip: skip degenerate or behind-camera triangles
        if (v0.w <= 0.0f || v1.w <= 0.0f || v2.w <= 0.0f) continue;

        // Perspective divide
        const float inv0 = 1.0f / v0.w;
        const float inv1 = 1.0f / v1.w;
        const float inv2 = 1.0f / v2.w;

        // NDC → screen pixels
        const float sx0 = (v0.x * inv0 + 1.0f) * hw;
        const float sy0 = (1.0f - v0.y * inv0) * hh;
        const float sz0 = v0.z * inv0;
        const float sx1 = (v1.x * inv1 + 1.0f) * hw;
        const float sy1 = (1.0f - v1.y * inv1) * hh;
        const float sz1 = v1.z * inv1;
        const float sx2 = (v2.x * inv2 + 1.0f) * hw;
        const float sy2 = (1.0f - v2.y * inv2) * hh;
        const float sz2 = v2.z * inv2;

        // Bounding box of triangle in screen space
        int minX = static_cast<int>(std::floor(std::min({sx0, sx1, sx2})));
        int minY = static_cast<int>(std::floor(std::min({sy0, sy1, sy2})));
        int maxX = static_cast<int>(std::ceil( std::max({sx0, sx1, sx2})));
        int maxY = static_cast<int>(std::ceil( std::max({sy0, sy1, sy2})));

        minX = std::max(minX, 0);
        minY = std::max(minY, 0);
        maxX = std::min(maxX, static_cast<int>(m_width)  - 1);
        maxY = std::min(maxY, static_cast<int>(m_height) - 1);

        // Edge function denominator for barycentric
        const float denom = (sy1 - sy2) * (sx0 - sx2) + (sx2 - sx1) * (sy0 - sy2);
        if (std::abs(denom) < 1e-6f) continue;
        const float inv_denom = 1.0f / denom;

        for (int py = minY; py <= maxY; ++py) {
            for (int px = minX; px <= maxX; ++px) {
                const float cx = static_cast<float>(px) + 0.5f;
                const float cy = static_cast<float>(py) + 0.5f;

                // Barycentric coordinates
                const float w0 = ((sy1 - sy2) * (cx - sx2) + (sx2 - sx1) * (cy - sy2)) * inv_denom;
                const float w1 = ((sy2 - sy0) * (cx - sx2) + (sx0 - sx2) * (cy - sy2)) * inv_denom;
                const float w2 = 1.0f - w0 - w1;

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

                const float depth = w0 * sz0 + w1 * sz1 + w2 * sz2;
                float& stored = m_depth_buffer[static_cast<u32>(py) * m_width + static_cast<u32>(px)];
                if (depth < stored) stored = depth;
            }
        }
    }
}

bool OcclusionCuller::IsOccluded(const AABB& bounds, const mat4& mvp) const {
    if (m_depth_buffer.empty()) return false;

    const float hw = static_cast<float>(m_width)  * 0.5f;
    const float hh = static_cast<float>(m_height) * 0.5f;

    // Generate 8 corners of the AABB
    const vec3 corners[8] = {
        {bounds.min.x, bounds.min.y, bounds.min.z},
        {bounds.max.x, bounds.min.y, bounds.min.z},
        {bounds.min.x, bounds.max.y, bounds.min.z},
        {bounds.max.x, bounds.max.y, bounds.min.z},
        {bounds.min.x, bounds.min.y, bounds.max.z},
        {bounds.max.x, bounds.min.y, bounds.max.z},
        {bounds.min.x, bounds.max.y, bounds.max.z},
        {bounds.max.x, bounds.max.y, bounds.max.z},
    };

    int min_sx = m_width, min_sy = m_height, max_sx = 0, max_sy = 0;
    float min_depth = 1.0f;
    bool any_in_front = false;

    for (const auto& c : corners) {
        vec4 clip = mvp * vec4(c, 1.0f);
        if (clip.w <= 0.0f) continue;
        any_in_front = true;
        const float inv_w = 1.0f / clip.w;
        const float nx = clip.x * inv_w;
        const float ny = clip.y * inv_w;
        const float nz = clip.z * inv_w;

        int sx = static_cast<int>((nx + 1.0f) * hw);
        int sy = static_cast<int>((1.0f - ny) * hh);
        min_sx = std::min(min_sx, sx);
        min_sy = std::min(min_sy, sy);
        max_sx = std::max(max_sx, sx);
        max_sy = std::max(max_sy, sy);
        if (nz < min_depth) min_depth = nz;
    }

    if (!any_in_front) return false;  // Behind camera — not culled

    // Clamp to screen
    min_sx = std::max(min_sx, 0);
    min_sy = std::max(min_sy, 0);
    max_sx = std::min(max_sx, static_cast<int>(m_width)  - 1);
    max_sy = std::min(max_sy, static_cast<int>(m_height) - 1);

    if (min_sx > max_sx || min_sy > max_sy) return false;

    // Sample depth buffer within the 2D screen rect.
    // If every sampled depth is LESS than our nearest corner, we are behind an occluder.
    for (int py = min_sy; py <= max_sy; ++py) {
        for (int px = min_sx; px <= max_sx; ++px) {
            const float occluder_depth = m_depth_buffer[static_cast<u32>(py) * m_width + static_cast<u32>(px)];
            if (occluder_depth >= min_depth) {
                // At least one pixel is not blocked
                return false;
            }
        }
    }

    return true;  // Fully behind occluder depth
}

void OcclusionCuller::Clear() {
    std::fill(m_depth_buffer.begin(), m_depth_buffer.end(), 1.0f);
}

} // namespace action
